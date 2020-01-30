#include "../../include/CRModel.h"

EffectiveCRModel::EffectiveCRModel(Metaparameters& meta):CRModel(meta){
  this->equations_of_evolution = &effective_ode_equations_of_evolution;
}
EffectiveCRModel::EffectiveCRModel(const foodmatrix& F, Metaparameters& meta):CRModel(F, meta){
  this->equations_of_evolution = &effective_ode_equations_of_evolution;
}
EffectiveCRModel::EffectiveCRModel(const CRModel & model){
  this->metaparameters = model.get_metaparameters();
  this->eq_vals=model.get_equilibrium_abundances();
  this->model_param=model.get_model_parameters();
  this->equations_of_evolution=&effective_ode_equations_of_evolution;
}

nmatrix EffectiveCRModel::jacobian(const Dynamical_variables& dyn_var) const{
  const Parameter_set* p = this->model_param->get_parameters();
  nmatrix J(p->NS, nvector(p->NS, 0.));
  std::cerr << "Effective jacobian at all times not implemented yet, please do not use this " << std::endl;
  abort();
  return J;
}

nmatrix EffectiveCRModel::jacobian_at_equilibrium() const{
  const Parameter_set* p = this->model_param->get_parameters();
  const nvector Seq = this->get_consumers_equilibrium();
  const nvector Req = this->get_resources_equilibrium();

  nmatrix J(p->NS, nvector(p->NS, 0.));
  for(size_t i=0; i<p->NS;++i){
    for(size_t j=0; j < p->NS;++j){
      ntype matrix_el=0.;
      for(size_t nu=0; nu < p->NR; ++nu){
        ntype denom=0.;
        for(size_t k=0; k < p->NS; ++k){
          denom+=p->gamma[k][nu]*Seq[k];
        }
        denom+=p->m[nu];
        ntype prefactor = (p->sigma[i][nu]*p->gamma[i][nu])/denom;
        matrix_el+= prefactor*(p->alpha[nu][j]-p->gamma[j][nu]*Req[nu]);
        if(i==j){
          matrix_el+=p->alpha[nu][i];
        }
      }
      J[i][j]=matrix_el;
    }
  }

  return J;
}
