#include "../../include/CRModel.h"
#include <iostream>
#include <fstream>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>

CRModel::CRModel(Model_parameters* mod_params){
  model_param=mod_params;
  eq_vals=NULL;
  metaparameters=NULL;
  return;
}
CRModel::CRModel(Metaparameters& meta){
  foodmatrix food_matrix = load_food_matrix(meta);
  *this = CRModel::CRModel(food_matrix, meta);
}
CRModel::CRModel(const foodmatrix& F, Metaparameters& meta){
  unsigned int attempts(0);
  this->model_param = new Model_parameters();
  Parameter_set* p = model_param->get_parameters();

  do{
    attempts+=1;

    nvector Req = build_resources(meta);
    nvector Seq = build_consumers(meta);

    this->eq_vals = new ntensor();
    nmatrix equilibria;
    equilibria.push_back(Req);
    equilibria.push_back(Seq);
    eq_vals->push_back(equilibria);

    p->NR = meta.NR;
    p->NS = meta.NS;

    // first sigma, Req, Seq are drawn randomly
    p->sigma=build_sigma(meta);

    // then we build gamma according to the food matrix
    p->gamma=build_gamma(F,meta);

    // then we build alpha according to the other parameters
    p->alpha=build_alpha(p, meta, Req, attempts);
    p->tau = build_tau(p, meta, attempts);

    // d is then set
    nvector d;
    for (size_t i=0; i < meta.NS; ++i){
      ntype result = 0.;
      for (size_t mu =0 ; mu < meta.NR; ++mu){
        result+=(p->sigma)[i][mu]*(p->gamma)[i][mu]*Req[mu]-(p->tau)[mu][i];
      }
      d.push_back(result);
    }
    p->d = d;

    // still have to set l and m
    nvector l, m;
    l = build_l(meta);
    for(size_t nu=0; nu < p->NR; ++nu){
      ntype C = 0.;
      for(size_t j = 0; j < p->NS; ++j){
        C+=(p->alpha[nu][j]*Seq[j]-p->gamma[j][nu]*Req[nu]*Seq[j]);
      }
      m.push_back(ntype(l[nu]+C)/Req[nu]);
    }

    // std::exponential_distribution<ntype> exp_distrib(1.);
    // for(size_t nu = 0; nu < p->NR; ++nu){
    //   ntype  C = 0.;
    //   for(size_t j = 0; j < p->NS; ++j){
    //     C+= (p->alpha[nu][j]-p->gamma[j][nu]*Req[nu])*Seq[j];
    //   }
    //   if(C > 0.){
    //     l.push_back(exp_distrib(random_device));
    //   }else{
    //     l.push_back(exp_distrib(random_device) - C);
    //   }
    //   m.push_back(ntype(l[nu]+C)/Req[nu]);
    // }

    p->l = l;
    p->m = m;
  }while(not(this->constraints_fulfilled(meta)));

  this->metaparameters = &meta;
  if(meta.verbose > 1){
    std::cout << "Feasible system build in "<<attempts<<" iteration(s). ";
    if(attempts > meta.nb_attempts){
      std::cout << "The metaparameters had to be changed.";
    }else{
      std::cout << "It was possible to use the initial metaparameters.";
    }
    std::cout << std::endl;
  }


  return;
}
nvector CRModel::equations_of_evolution(const Dynamical_variables& dyn_var) const{
  nvector v;
  const Parameter_set* p = this->model_param->get_parameters();
  const nvector R = *dyn_var.get_resources();
  const nvector S = *dyn_var.get_consumers();

  for (size_t nu=0; nu < p->NR; ++nu){
    ntype result(0.);
    result+=p->l[nu];
    result-=p->m[nu]*R[nu];
    for (size_t j=0; j < p->NS; ++j){
      result-=p->gamma[j][nu]*R[nu]*S[j];
      result+=p->alpha[nu][j]*S[j];
    }
    v.push_back(result);
  }

  for (size_t i=0; i<p->NS; ++i){
    ntype result=0.;
    for (size_t mu=0; mu < p->NR; ++mu){
      result+=p->sigma[i][mu]*p->gamma[i][mu]*S[i]*R[mu];
      result-=p->tau[mu][i]*S[i];
    }
    result-=p->d[i]*S[i];
    v.push_back(result);
  }
  return v;
}
nmatrix CRModel::jacobian(const Dynamical_variables& dyn_var) const{
  const Parameter_set* p = this->model_param->get_parameters();
  const nvector R = *dyn_var.get_resources();
  const nvector S = *dyn_var.get_consumers();

  nmatrix J(p->NR+p->NS, nvector(p->NR+p->NS, 0.));
  // fills the dR/dR part (should always be diagonal since a resource never directly depends on another)
  for(size_t nu=0; nu < p->NR; ++nu){
    J[nu][nu] = -p->m[nu];
    for(size_t j=0; j < p->NS; ++j){
      J[nu][nu] -= p->gamma[j][nu]*S[j];
    }
  }

  // fills the dR/dS part (upper right part of jacobian)
  for(size_t nu=0; nu < p->NR;++nu){
    for(size_t i=0; i < p->NS; ++i){
      J[nu][i+p->NR] = p->alpha[nu][i]-p->gamma[i][nu]*R[nu];
    }
  }

  // fills the dS/dR part (lower left)
  for(size_t i=0; i < p->NS; ++i){
    for(size_t nu=0; nu < p->NR; ++nu){
      J[p->NR+i][nu] = p->sigma[i][nu]*p->gamma[i][nu]*S[i];
    }
  }

  // finally, fills the dR/dR part (lower right). It is diagonal for the same reason
  for (size_t i=0; i < p->NS; ++i){
    for(size_t mu=0; mu < p->NR; ++mu){
      J[i+p->NR][i+p->NR] += p->sigma[i][mu]*p->gamma[i][mu]*R[mu]-p->tau[mu][i];
    }
    J[i+p->NR][i+p->NR] -= p->d[i];
  }

  return J;
}
nmatrix CRModel::jacobian_at_equilibrium() const{
  nvector Req = (*eq_vals)[0][0];
  nvector Seq = (*eq_vals)[0][1];

  Dynamical_variables dyn_var(&Req, &Seq);
  nmatrix jac_eq = jacobian(dyn_var);
  return jac_eq;
}
ncvector CRModel::eigenvalues_at_equilibrium() const{
  ncvector v;
  nmatrix jac_eq = this->jacobian_at_equilibrium();
  ntype min_element(std::abs(jac_eq[0][0]));

  for(size_t i = 0; i < jac_eq.size(); ++i){
    for(size_t j=0; j < jac_eq[i].size(); ++j){
      if(jac_eq[i][j]*jac_eq[i][j] > 0. and std::abs(jac_eq[i][j]) < min_element){
        min_element = std::abs(jac_eq[i][j]);
      }
    }
  }

  // for testing purpose
  //min_element = 1.;
  //std::cout << " put min_element as 1" << std::endl;

  const unsigned int NR = this->model_param->get_parameters()->NR;
  const unsigned int NS = this->model_param->get_parameters()->NS;

  Eigen::Matrix<ntype, Eigen::Dynamic, Eigen::Dynamic> jacob;
  jacob.resize(NR+NS, NR+NS);
  // we rescale the jacobian such that even the smallest value is of order 1
  for(size_t i=0; i < NR+NS; ++i){
    for(size_t j=0; j < NR+NS; ++j){
      jacob(i,j) = jac_eq[i][j]/min_element;
    }
  }
  Eigen::Matrix<nctype, Eigen::Dynamic, 1> eivals = jacob.eigenvalues();
  for(size_t i=0; i < eivals.rows(); ++i){
    v.push_back(eivals(i)*min_element);
    //v.push_back(eivals(i));
  }
  return v;
}
void CRModel::save(std::ostream& os) const{
  std::cout << " Still have to implement this, bye" << std::endl;
  return;
}
std::ostream& CRModel::display(std::ostream& os) const{
  os << "The model is defined by the following parameters :" << std::endl;
  os << *model_param << std::endl;
  os << "The following equilibrium values have been found :" << std::endl;
  os << *eq_vals << std::endl;
  os << "The model is characterised by the following metaparameters" << std::endl;
  os << *metaparameters << std::endl;
  return os;
}
bool CRModel::constraints_fulfilled(const Metaparameters& m) const{
  if(not(this->positive_parameters())){
    return false;
  }
  if(m.energy_constraint and not(this->energy_constraint())){
    return false;
  }
  return true;
}
bool CRModel::energy_constraint() const{
  Parameter_set* p = this->model_param->get_parameters();
  for(size_t j = 0 ; j < this->eq_vals->size();++j){
    nvector Req = (*eq_vals)[j][0];
    for(size_t i = 0; i < p->NS; ++i){
      ntype somme = 0.;
      for(size_t nu=0; nu < p->NR; ++nu){
        somme+=(1-p->sigma[i][nu])*p->gamma[i][nu]*Req[nu];
        somme-=p->alpha[nu][i];
      }
      if(somme < 0.){
        return false;
      }
    }
  }
  return true;
}
bool CRModel::positive_parameters() const{
  Parameter_set* p = this->model_param->get_parameters();
  if(not(non_neg_elements(p->d))){
    std::cerr << "d contains negative elements" << std::endl;
    return false;
  }
  if(not(non_neg_elements(p->l))){
    std::cerr << "l contains negative elements" << std::endl;
    return false;
  }
  if(not(non_neg_elements(p->m))){
    std::cerr << "m contains negative elements : ";
    std::cerr << p->m << std::endl;
    return false;
  }
  return true;
}
bool CRModel::dynamically_stable() const{
  ncvector v = this->eigenvalues_at_equilibrium();
  for(size_t i = 0; i< v.size(); ++i){
    if(norm(v[i])>EIGENSOLVER_PRECISION){
      return false;
    }
  }
  return true;
}
void CRModel::save_simulation() const{
  std::ofstream myfile;
  Metaparameters* m = this->metaparameters;
  myfile.open(m->save_path,std::ios_base::app);
  bool save_success(false);
  if(not(myfile.is_open())){
    std::cerr << "Could not open "<<m->save_path <<" for saving the simulation "<< std::endl;
  }else{
    save_success=true;
    time_t now=time(0);
    /*
    tm* ltm= localtime(&now);
    myfile << 1900+ltm->tm_year << " " << 1+ltm->tm_mon << " " << ltm->tm_mday<<" ";
    myfile << 1+ltm->tm_hour << ":"<<1+ltm->tm_min << ":"<<1+ltm->tm_sec<<" ";
    */
    myfile << "# "  << m->p << " " << m->epsilon << " " << m->foodmatrixpath << " " << m->verbose << " ";
    myfile << m->energy_constraint << " " << m->budget_constraint << " " << m->nb_attempts<<" " << m->seed_number <<" ";
    myfile << m->save_path << " ";
    myfile << ctime(&now) ;
    myfile << m->gamma0 <<" " <<m->alpha0 << " " << m->sigma0 << " ";
    myfile << m->p << " " << m->R0 << " " << m->S0 << " " <<m->l0<<" "<< m->NR << " ";
    myfile << m->NS;
    ncvector eigenvalues = this->eigenvalues_at_equilibrium();
    for(size_t i=0; i<eigenvalues.size(); ++i){
      myfile << " " << eigenvalues[i];
    }
    myfile << std::endl;
  }
  if(m->verbose > 0){
    if(save_success){
      std::cout << "Successfully saved model to " << m->save_path << std::endl;
    }
  }
  myfile.close();
  return;
}
void CRModel::save_jacobian_at_equilibrium(std::string savepath) const{
  Metaparameters* m = this->metaparameters;
  std::ofstream myfile, myfile2;
  myfile.open(savepath,std::ios_base::app);
  myfile2.open(savepath+".meta", std::ios_base::app);
  bool save_success(false);
  if(not(myfile.is_open()) or not(myfile2.is_open())){
    std::cerr << "Could not open "<<savepath <<" for saving the simulation "<< std::endl;
  }else{
    save_success=true;
    time_t now=time(0);
    /*
    tm* ltm= localtime(&now);
    myfile << 1900+ltm->tm_year << " " << 1+ltm->tm_mon << " " << ltm->tm_mday<<" ";
    myfile << 1+ltm->tm_hour << ":"<<1+ltm->tm_min << ":"<<1+ltm->tm_sec<<" ";
    */
    myfile2 << "# "  << m->p << " " << m->epsilon << " " << m->foodmatrixpath << " " << m->verbose << " ";
    myfile2 << m->energy_constraint << " " << m->budget_constraint << " " << m->nb_attempts<<" " << m->seed_number <<" ";
    myfile2 << savepath << " ";
    myfile2 << ctime(&now) ;
    myfile2 << m->gamma0 <<" " <<m->alpha0 << " " << m->sigma0 << " ";
    myfile2 << m->p << " " << m->R0 << " " << m->S0 << " " <<m->l0<<" "<< m->NR << " ";
    myfile2 << m->NS << std::endl;

    myfile << this->jacobian_at_equilibrium() << "#"<< std::endl;

  }
  if(m->verbose > 0){
    if(save_success){
      std::cout << "Successfully saved model to " << savepath << std::endl;
    }
  }
  myfile.close();
  return;
}
void CRModel::write_death_rates(std::string savepath) const{
  std::ofstream myfile, myfile2, myfile3;
  Parameter_set* p = this->model_param->get_parameters();
  myfile.open(savepath+".resources", std::ios_base::app);
  if(not(myfile.is_open())){
    std::cerr << "Could not open "<<savepath <<" for saving the resources death rates "<< std::endl;
  }else{
    for(size_t nu = 0; nu < p->NR; ++nu){
      myfile << p->m[nu] << " ";
    }
    myfile<<std::endl;
  }

  myfile2.open(savepath+".consumers", std::ios_base::app);
  if(not(myfile2.is_open())){
    std::cerr << "Could not open "<<savepath <<" for saving the consumers death rates "<< std::endl;
  }else{
    for(size_t i = 0; i < p->NS; ++i){
      myfile2 << p->d[i] << " ";
    }
    myfile2 << std::endl;
  }

  Metaparameters* m = this->metaparameters;
  myfile3.open(savepath+".meta", std::ios_base::app);
  if(not(myfile3.is_open())){
    std::cerr << "Could not open "<<savepath <<" for saving the metaparameters of death rates "<< std::endl;
  }else{
    myfile3 << m->gamma0 <<" " <<m->alpha0 << " " << m->sigma0 << " ";
    myfile3 << m->p << " " << m->R0 << " " << m->S0 << " " <<m->l0<<" "<< m->NR << " ";
    myfile3 << m->NS << std::endl;
  }

  myfile.close();
  myfile2.close();
  myfile3.close();

  return;
}
void CRModel::write_time_evolution(const Dynamical_variables & dyn, ntype tf) const{
  Metaparameters* m = this->metaparameters;
  std::ofstream myfile;
  myfile.open(m->save_path, std::ios::app);
  bool save_success(false);
  if(not(myfile.is_open())){
    std::cerr << "Could not open " << m->save_path << " to write the temporal evolution" << std::endl;
  }else{
    save_success = true;
  }

  nmatrix evolution = this->time_evolution(dyn, tf);
  for(size_t i =0; i < evolution.size() ; ++i){
    myfile << evolution[i] << std::endl;
  }

  if(m->verbose > 0){
    if(save_success){
      std::cout << "Successfully saved temporal evolution of system to " << m->save_path << std::endl;
    }
  }
  myfile.close();
  return;
}
void CRModel::write_time_evolution_from_equilibrium() const{
  nvector* eq_resources = &(*eq_vals)[0][0];
  nvector* eq_consumers = &(*eq_vals)[0][1];

  Dynamical_variables dyn(eq_resources, eq_consumers);
  write_time_evolution(dyn, this->metaparameters->tf);
  return ;
}
Dynamical_variables CRModel::perturb_equilibrium() const{
  nvector unp_resources = (*eq_vals)[0][0];
  nvector unp_consumers = (*eq_vals)[0][1];

  nvector* p_resources = new nvector;
  nvector* p_consumers = new nvector;

  ntype eps = this->metaparameters->perturb_eq;

  // we perturb the equilibrium values
  for(size_t nu=0; nu < unp_resources.size(); ++nu){
    std::uniform_real_distribution<ntype> resource_dist((1.-eps)*unp_resources[nu], (1.+eps)*unp_resources[nu]);
    p_resources->push_back(resource_dist(random_engine));
  }

  for(size_t i = 0; i < unp_consumers.size(); ++i){
    std::uniform_real_distribution<ntype> consumer_dist((1.-eps)*unp_consumers[i], (1.+eps)*unp_consumers[i]);
    p_consumers->push_back(consumer_dist(random_engine));
  }

  return Dynamical_variables(p_resources, p_consumers);

}
void CRModel::perturb_parameters(const ntype & Delta) const{
  Parameter_set* p = this->model_param->get_parameters();
  std::uniform_real_distribution<ntype> uniform_distrib(-1., 1.);

  for(size_t mu=0; mu < p->l.size(); ++mu){
    p->l[mu] = p->l[mu]*(1+Delta*uniform_distrib(random_engine));
  }

  if(this->metaparameters->verbose > 1){
    std::cout <<" Structurally perturbed the system with parameter delta =" << Delta << std::endl;
  }

  return;
}
void CRModel::perturb_parameters() const{
  this->perturb_parameters(this->metaparameters->perturb_parameters);
  return;
}
void CRModel::save_new_equilibrium(const Extinction& ext) const{
  Metaparameters* m = this->metaparameters;
  std::ofstream myfile;
  myfile.open(m->save_path, std::ios::app);
  bool save_success(false);
  if(not(myfile.is_open())){
    std::cerr << "Could not open " << m->save_path << " to write the new equilibrium of the system" << std::endl;
  }else{
    save_success = true;
  }
  myfile << "# "  << m->p << " " << m->epsilon << " " << m->foodmatrixpath << " " << m->verbose << " ";
  myfile << m->energy_constraint << " " << m->budget_constraint << " " << m->nb_attempts<<" " << m->seed_number <<" ";
  myfile << m->save_path << std::endl;
  myfile << m->NR << " " << m->NS << " " << m->perturb_parameters << " " << ext.t_eq << " " << ext.extinct  << std::endl;

  if(m->verbose > 0){
    if(save_success){
      std::cout << "Successfully saved new equilibrium of system in "<< m->save_path << std::endl;
    }
  }

  myfile.close();

  return;
}