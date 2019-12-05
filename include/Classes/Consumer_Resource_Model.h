#ifndef CONSUMER_RESOURCE_MODEL_H
#define CONSUMER_RESOURCE_MODEL_H

#include "Custom_types.h"
#include "Model_Parameters.h"
#include "Metaparameters.h"
#include "Dynamical_variables.h"
#include "Extinction.h"
#include <iostream>
#include <string>

class CRModel{
private:
  Metaparameters* metaparameters;
  ntensor* eq_vals; // we allow the possibility of multiple equilibria (hence vector of vector)
  Model_parameters* model_param;

public:
  CRModel(Metaparameters&);
  CRModel(const foodmatrix&, Metaparameters&);
  CRModel(Model_parameters*);
  nvector equations_of_evolution(const Dynamical_variables&) const; // returns the value of the RHS of the equations of evolution
  nmatrix jacobian_at_equilibrium() const;
  ncvector eigenvalues_at_equilibrium() const;
  nmatrix jacobian(const Dynamical_variables&) const; // returns the jacobian for the given dynamical variables
  void save(std::ostream&) const; // outputs the model to the external file
  std::ostream& display(std::ostream&) const;
  bool energy_constraint() const;
  bool constraints_fulfilled(const Metaparameters& m) const;
  bool positive_parameters() const;
  bool dynamically_stable() const;
  void save_simulation() const;
  void save_jacobian_at_equilibrium(std::string) const;
  void write_time_evolution(const Dynamical_variables&, ntype) const;
  void write_time_evolution_from_equilibrium() const;
  void write_time_evolution_until_equilibrium(const Dynamical_variables &, ntype, ntype) const;
  void write_death_rates(std::string) const;
  nmatrix time_evolution(const Dynamical_variables&, ntype) const ;
  Dynamical_variables perturb_equilibrium() const;
  void perturb_parameters() const;
  void perturb_parameters(const ntype &) const;
  Extinction evolve_until_equilibrium(ntype, eqmode eq_mode=convergence) const;
  void save_new_equilibrium(const Extinction&) const;
};

#endif