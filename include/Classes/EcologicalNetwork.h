#ifndef ECOLOGICALNETWORK_H
#define ECOLOGICALNETWORK_H

#include "../Classes/Metaparameters.h"
#include "../Classes/Custom_types.h"
#include <iostream>
#include <string>

struct EcologicalNetwork{
  /* Different ways of initializing an EcologicalNetwork */
  EcologicalNetwork();
  EcologicalNetwork(const unsigned int NR, const unsigned int NS, const double & g_conn);
  EcologicalNetwork(const Metaparameters& m);

  void optimize(MonteCarloSolver& mcs);

  // attributes
  nmatrix A;
  nmatrix G;
  unsigned int NR;
  unsigned int NS;
};




#endif