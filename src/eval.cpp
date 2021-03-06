#include <gsl/gsl_linalg.h>
#include "eval.h"
#include "weight.h"
#include "util.h"
#include "logger.h"
#include "logsumexp.h"

// if exit "probabilities" sum to more than this when trying to eliminate states using matrix algebra, issue a warning
#define SuspiciouslyLargeProbabilityWarningThreshold 1.01

EvaluatedMachine::EvaluatedMachine (const Machine& machine, const Params& params) :
  inputTokenizer (machine.inputAlphabet()),
  outputTokenizer (machine.outputAlphabet()),
  state (machine.nStates())
{
  init (machine, &params);
}

EvaluatedMachine::EvaluatedMachine (const Machine& machine) :
  inputTokenizer (machine.inputAlphabet()),
  outputTokenizer (machine.outputAlphabet()),
  state (machine.nStates())
{
  init (machine, NULL);
}

void EvaluatedMachine::init (const Machine& machine, const Params* params)
{
  Assert (machine.isAdvancingMachine(), "Machine is not topologically sorted");

  ProgressLog(plog,6);
  plog.initProgress ("Evaluating transition weights");

  EvaluatedMachineState::TransIndex tiCum = 0;
  for (StateIndex s = 0; s < nStates(); ++s) {
    plog.logProgress (s / (double) nStates(), "state %lu/%lu", s, nStates());
    state[s].name = machine.state[s].name;
    EvaluatedMachineState::TransIndex ti = 0;
    for (const auto& trans: machine.state[s].trans) {
      const StateIndex d = trans.dest;
      const InputToken in = inputTokenizer.sym2tok.at (trans.in);
      const OutputToken out = outputTokenizer.sym2tok.at (trans.out);
      const LogWeight lw = params ? log (WeightAlgebra::eval (trans.weight, params->defs)) : 0.;
      state[s].outgoing[in][out].insert (EvaluatedMachineState::StateTransMap::value_type (d, EvaluatedMachineState::Trans ({ .logWeight = lw, .transIndex = ti})));
      state[d].incoming[in][out].insert (EvaluatedMachineState::StateTransMap::value_type (s, EvaluatedMachineState::Trans ({ .logWeight = lw, .transIndex = ti})));
      ++ti;
    }
    state[s].nTransitions = ti;
    state[s].transOffset = tiCum;
    tiCum += ti;
  }
  nTransitions = tiCum;
}

StateIndex EvaluatedMachine::nStates() const {
  return state.size();
}

StateIndex EvaluatedMachine::startState() const {
  Assert (nStates() > 0, "EvaluatedMachine has no states");
  return 0;
}

StateIndex EvaluatedMachine::endState() const {
  Assert (nStates() > 0, "EvaluatedMachine has no states");
  return nStates() - 1;
}

void EvaluatedMachine::writeJson (ostream& out) const {
  out << "{\"state\":" << endl << " [";
  for (StateIndex s = 0; s < nStates(); ++s) {
    const EvaluatedMachineState& ms = state[s];
    out << (s ? "  " : "") << "{\"n\":" << s;
    if (!ms.name.is_null())
      out << "," << endl << "   \"id\":" << ms.name;
    if (ms.incoming.size()) {
      out << "," << endl << "   \"incoming\":[";
      size_t nt = 0;
      for (const auto& iost: ms.incoming)
	for (const auto& ost: iost.second)
	  for (const auto& st: ost.second) {
	    const auto& t = st.second;
	    if (nt++)
	      out << "," << endl << "               ";
	    out << "{\"from\":" << st.first;
	    if (iost.first) out << ",\"in\":\"" << inputTokenizer.tok2sym[iost.first] << "\"";
	    if (ost.first) out << ",\"out\":\"" << outputTokenizer.tok2sym[ost.first] << "\"";
	    out << ",\"logWeight\":" << t.logWeight;
	    out << "}";
	  }
      out << "]";
    }
    if (ms.outgoing.size()) {
      out << "," << endl << "   \"outgoing\":[";
      size_t nt = 0;
      for (const auto& iost: ms.outgoing)
	for (const auto& ost: iost.second)
	  for (const auto& st: ost.second) {
	    const auto& t = st.second;
	    if (nt++)
	      out << "," << endl << "               ";
	    out << "{\"to\":" << st.first;
	    if (iost.first) out << ",\"in\":\"" << inputTokenizer.tok2sym[iost.first] << "\"";
	    if (ost.first) out << ",\"out\":\"" << outputTokenizer.tok2sym[ost.first] << "\"";
	    out << ",\"logWeight\":" << t.logWeight;
	    out << "}";
	  }
      out << "]";
    }
    out << "}";
    if (s < nStates() - 1)
      out << "," << endl;
  }
  out << endl << " ]" << endl << "}" << endl;
}

string EvaluatedMachine::toJsonString() const {
  ostringstream outs;
  writeJson (outs);
  return outs.str();
}

string EvaluatedMachine::stateNameJson (StateIndex s) const {
  if (state[s].name.is_null())
    return to_string(s);
  return state[s].name.dump();
}

vguard<vguard<LogWeight> > EvaluatedMachine::sumInTrans() const {
  const OutputToken nullToken = outputTokenizer.emptyToken();

  vguard<vguard<double> > oneMinusNullTrans (nStates(), vguard<double> (nStates()));
  vguard<double> pExit (nStates(), 0.);
  for (StateIndex src = 0; src < nStates(); ++src) {
    oneMinusNullTrans[src][src] = 1;
    for (const auto& in_ost: state[src].outgoing) {
      const auto& ost = in_ost.second;
      if (ost.count(nullToken))
	for (const auto& s_t: ost.at(nullToken)) {
	  const double p = exp (s_t.second.logWeight);
	  oneMinusNullTrans[src][s_t.first] -= p;
	  pExit[src] += p;
	  if (pExit[src] > SuspiciouslyLargeProbabilityWarningThreshold)
	    LogThisAt (6, "Warning: when eliminating absorbing transitions, pExit[" << src << "] = " << pExit[src] << endl);
	}
    }
  }

  gsl_matrix* gOneMinusNullTrans = stl_to_gsl_matrix (oneMinusNullTrans);
  gsl_matrix* gGeomSumNullTrans = gsl_matrix_alloc (nStates(), nStates());
  gsl_permutation* perm = gsl_permutation_alloc (nStates());
  int signum;

  gsl_linalg_LU_decomp (gOneMinusNullTrans, perm, &signum);
  gsl_linalg_LU_invert (gOneMinusNullTrans, perm, gGeomSumNullTrans);
 
  const vguard<vguard<double> > result = gsl_matrix_to_stl (gGeomSumNullTrans);

  gsl_permutation_free (perm);
  gsl_matrix_free (gOneMinusNullTrans);
  gsl_matrix_free (gGeomSumNullTrans);
  
  return log_matrix (result);
}

Machine EvaluatedMachine::explicitMachine() const {
  Machine m;
  m.state = vguard<MachineState> (nStates());
  vguard<MachineState>::iterator iter = m.state.begin();
  for (const auto& ems: state) {
    MachineState& ms = *(iter++);
    ms.name = ems.name;
    for (const auto& i_ostm: ems.outgoing)
      for (const auto& o_stm: i_ostm.second)
	for (const auto& s_t: o_stm.second)
	  ms.trans.push_back (MachineTransition (inputTokenizer.tok2sym[i_ostm.first],
						 outputTokenizer.tok2sym[o_stm.first],
						 s_t.first,
						 exp (s_t.second.logWeight)));
  }
  return m;
}

											     
											   
