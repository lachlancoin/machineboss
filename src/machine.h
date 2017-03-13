#ifndef MACHINE_INCLUDED
#define MACHINE_INCLUDED

#include <string>
#include <map>
#include <set>
#include <list>
#include "jsonio.h"
#include "weight.h"
#include "vguard.h"

using namespace std;
using json = nlohmann::json;

typedef unsigned long long StateIndex;

#define MachineWaitTag "wait"
#define MachineSilentTag "silent"

typedef string OutputSymbol;
typedef string InputSymbol;
typedef json StateName;

struct MachineTransition {
  InputSymbol in;
  OutputSymbol out;
  StateIndex dest;
  WeightExpr weight;
  MachineTransition();
  MachineTransition (InputSymbol, OutputSymbol, StateIndex, WeightExpr);
  bool inputEmpty() const;
  bool outputEmpty() const;
  bool isSilent() const;  // inputEmpty() && outputEmpty()
  bool isLoud() const;  // !isSilent()
};
typedef list<MachineTransition> TransList;

struct MachinePath {
  TransList trans;
  void writeJson (ostream&) const;
};

struct MachineState {
  StateName name;
  TransList trans;
  MachineState();
  MachineTransition getTransition (size_t) const;
  bool exitsWithInput() const;  // true if this has an input transition
  bool exitsWithoutInput() const;  // true if this has a non-input transition
  bool exitsWithIO() const;  // true if this has any transitions with input and/or output
  bool exitsWithoutIO() const;  // true if this has any transitions without input or output
  bool terminates() const;  // true if this has no outgoing transitions. Note that the end state is not required to have this property
  bool waits() const;  // !exitsWithoutInput()
  bool continues() const;  // !exitsWithInput() && !terminates()
  bool isSilent() const;  // !exitsWithIO()
  bool isLoud() const;  // exitsWithIO() && !exitsWithoutIO()
};

struct Machine {
  vguard<MachineState> state;

  void writeJson (ostream& out) const;
  void readJson (const json& json);

  StateIndex nStates() const;
  size_t nTransitions() const;
  StateIndex startState() const;
  StateIndex endState() const;

  vguard<InputSymbol> inputAlphabet() const;
  vguard<OutputSymbol> outputAlphabet() const;

  set<StateIndex> accessibleStates() const;

  static Machine null();

  static Machine compose (const Machine& first, const Machine& second);
  static Machine concatenate (const Machine& left, const Machine& right);
  static Machine generator (const string& name, const vguard<OutputSymbol>& seq);
  static Machine acceptor (const string& name, const vguard<InputSymbol>& seq);

  static Machine unionOf (const Machine& first, const Machine& second);
  static Machine unionOf (const Machine& first, const Machine& second, const WeightExpr& pFirst);
  static Machine unionOf (const Machine& first, const Machine& second, const WeightExpr& pFirst, const WeightExpr& pSecond);

  Machine kleeneClosure() const;
  Machine kleeneClosure (const WeightExpr& extend) const;
  Machine kleeneClosure (const WeightExpr& extend, const WeightExpr& end) const;

  Machine reverse() const;
  Machine flipInOut() const;
  
  bool isErgodicMachine() const;  // all states accessible
  bool isWaitingMachine() const;  // all states wait or continue
  bool isAdvancingMachine() const;  // no silent i->j transitions where j<i
  bool isAligningMachine() const;  // at most i->j transition with given input & output labels

  Machine ergodicMachine() const;  // remove unreachable states
  Machine waitingMachine() const;  // convert to waiting machine
  Machine advancingMachine() const;  // convert to advancing machine
};

typedef JsonLoader<Machine> MachineLoader;

struct TransAccumulator {
  map<StateIndex,map<InputSymbol,map<OutputSymbol,WeightExpr> > > t;
  void accumulate (InputSymbol in, OutputSymbol out, StateIndex dest, WeightExpr w);
  TransList transitions() const;
};

#endif /* MACHINE_INCLUDED */
