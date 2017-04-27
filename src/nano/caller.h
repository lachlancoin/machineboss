#ifndef BASECALL_INCLUDED
#define BASECALL_INCLUDED

#include "../machine.h"
#include "../fastseq.h"
#include "prior.h"

struct BaseCallingParamNamer {
  static string emitLabel (const string& kmerStr);
  static string condFreqLabel (const string& prefix, const char suffix);
  static string cptWeightLabel (const string& kmerStr, int cpt);
  static string cptExtendLabel (const string& kmerStr, int cpt);
  static string cptEndLabel (const string& kmerStr, int cpt);
  static string cptName (int cpt);
};

struct BaseCallingParams : BaseCallingParamNamer {
  string alphabet;
  SeqIdx kmerLen;
  int components;
  GaussianModelParams params;
  void init (const string& alph, SeqIdx kmerLen, int components);
  json asJson() const;
  void writeJson (ostream& out) const;
  void readJson (const json& json);
};

struct BaseCallingPrior : BaseCallingParamNamer {
  double condFreq, cptWeight, cptExtend, cptEnd;
  double mu, muCount;
  double tau, tauCount;

  BaseCallingPrior();
  
  GaussianModelPrior modelPrior (const string& alph, SeqIdx kmerLen, int components) const;
};

struct BaseCallingMachine : Machine, BaseCallingParamNamer {
  int components, nKmers;
  void init (const string& alph, SeqIdx kmerLen, int components);
  // State indices are organized so that the only backward transitions (i->j where j<i) are output emissions
  inline StateIndex kmerEmit (Kmer kmer, int component) const { return 1 + component * nKmers + kmer; }
  inline StateIndex kmerEnd (Kmer kmer) const { return 1 + components * nKmers + kmer; }
  inline StateIndex kmerStart (Kmer kmer) const { return 1 + (components + 1) * nKmers + kmer; }
};

#endif /* BASECALL_INCLUDED */
