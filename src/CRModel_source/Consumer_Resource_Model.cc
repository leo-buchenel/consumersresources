#include "../../include/CRModel.h"
#include <iostream>
#include <fstream>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#include <algorithm>

/*  default constructor, every pointer is initialized to NULL */
CRModel::CRModel(){
  this->metaparameters = NULL;
  this->eq_vals = NULL;
  this->model_param = NULL;
  this->equations_of_evolution = ode_equations_of_evolution;
  return;
}
CRModel::CRModel(const CRModel& model){
  this->metaparameters= &(*(model.get_metaparameters()));
  this->eq_vals = new ntensor(*(model.get_equilibrium_abundances()));
  this->model_param = new Model_parameters(*(model.get_model_parameters()));
  this->equations_of_evolution = func_equ_evol(model.get_equations_of_evolution());
}
CRModel::CRModel(Model_parameters* mod_params):CRModel(){
  model_param=mod_params;
  return;
}
CRModel::CRModel(Metaparameters& meta):CRModel(load_food_matrix(meta), meta){
  return;
}
CRModel::CRModel(const foodmatrix& F, Metaparameters& meta):equations_of_evolution(ode_equations_of_evolution){
  unsigned int attempts(0);
  this->create_model_parameters(meta);
  do{
    attempts+=1;
    switch(meta.building_mode){
      case use_l:{
        this->attempt_to_build_model(F, meta, attempts);
        break;
      }
      case use_m:{
        this->attempt_to_build_model_with_m(F, meta, attempts);
        break;
      }
      default:{
        throw error("Unrecognized building mode in model constructor.");
      }
    }
  }while(not(this->constraints_fulfilled(meta)));

  if(meta.verbose > 1){
    std::cout << "\t Feasible system built in "<<attempts<<" iteration(s). ";
    if(attempts > meta.nb_attempts){
      std::cout << "\t The metaparameters had to be changed.";
    }else{
      std::cout << "It was possible to use the initial metaparameters.";
    }
    std::cout << std::endl;
  }
  return;
}
CRModel::~CRModel(){
  delete this->model_param;
  delete this->eq_vals;
  return;
}
CRModel& CRModel::operator=(const CRModel& other){
  if(&other!=this){
    this->metaparameters= &(*(other.get_metaparameters()));
    /* first delete the old pointers and then build them again */
    delete this->eq_vals;
    this->eq_vals = new ntensor(*(other.get_equilibrium_abundances()));
    delete this->model_param;
    this->model_param = new Model_parameters(*(other.get_model_parameters()));
    this->equations_of_evolution = func_equ_evol(other.get_equations_of_evolution());
  }
  return *this;
}
void CRModel::create_model_parameters(Metaparameters& meta){
  this->model_param = new Model_parameters();
  this->eq_vals = new ntensor();
  this->metaparameters = &meta;
  return;
}
void CRModel::attempt_to_build_model(const foodmatrix& F, Metaparameters& meta, unsigned int attempts){
  Parameter_set* p = this->model_param->get_parameters();
  if(meta.verbose>3){
    std::cout << "\t \t \t Build system choosing l " << std::endl;
  }
  /* first find the values for the equilibria */
  nvector Req = build_resources(meta);
  nvector Seq = build_consumers(meta);

  nmatrix equilibria;
  equilibria.push_back(Req);
  equilibria.push_back(Seq);

  ntensor equilibria_vals;
  equilibria_vals.push_back(equilibria);

  *(this->eq_vals) = equilibria_vals;

  p->NR = meta.NR;
  p->NS = meta.NS;

  /* first sigma, Req, Seq are drawn randomly */
  p->sigma = build_sigma_Butler(meta);

  /* then we build gamma according to the food matrix */
  p->gamma = build_gamma(F,meta);

  /* then we build alpha according to the other parameters */
  p->alpha = build_alpha(p, meta, Req, attempts);
  p->tau = build_tau(p, meta, attempts);

  /* d is then set */
  nvector d;
  for (size_t i=0; i < meta.NS; ++i){
    ntype result = 0.;
    for (size_t mu =0 ; mu < meta.NR; ++mu){
      result+=(p->sigma)[i][mu]*(p->gamma)[i][mu]*Req[mu]-(p->tau)[mu][i];
    }
    d.push_back(result);
  }
  p->d = d;

  /* still have to set l and m */
  nvector l, m(meta.NR);
  l = build_l(meta);
  for(size_t nu=0; nu < p->NR; ++nu){
    ntype C = 0.;
    for(size_t j = 0; j < p->NS; ++j){
      C+=(p->alpha[nu][j]*Seq[j]-p->gamma[j][nu]*Req[nu]*Seq[j]);
    }
    m[nu]=(ntype(l[nu]+C)/Req[nu]);
  }
  p->l = l;
  p->m = m;

  return;
}
void CRModel::attempt_to_build_model_with_m(const foodmatrix& F, Metaparameters& meta, unsigned int attempts){
  Parameter_set* p = this->model_param->get_parameters();

  if(meta.verbose>3){
    std::cout << "\t \t \t Build system choosing m " << std::endl;
  }

  /* first find the values for the equilibria */
  nvector Req = build_resources(meta);
  nvector Seq = build_consumers(meta);

  nmatrix equilibria;
  equilibria.push_back(Req);
  equilibria.push_back(Seq);

  ntensor equilibria_vals;
  equilibria_vals.push_back(equilibria);

  *(this->eq_vals) = equilibria_vals;

  p->NR = meta.NR;
  p->NS = meta.NS;

  /* then sigma */
  p->sigma = build_sigma_Butler(meta);

  /* then we build gamma according to the food matrix */
  p->gamma = build_gamma(F,meta);

  /* then we build alpha according to the other parameters */
  p->alpha = build_alpha(p, meta, Req, attempts);
  p->tau = build_tau(p, meta, attempts);

  /* d is then set */
  nvector d;
  for (size_t i=0; i < meta.NS; ++i){
    ntype result = 0.;
    for (size_t mu =0 ; mu < meta.NR; ++mu){
      result+=(p->sigma)[i][mu]*(p->gamma)[i][mu]*Req[mu]-(p->tau)[mu][i];
    }
    d.push_back(result);
  }
  p->d = d;

  /* still have to set l and m */
  nvector l(meta.NR, 0.), m(meta.NR, 0.);
  m = build_m(meta);
  for(size_t nu=0; nu < p->NR; ++nu){
    l[nu]=m[nu]*Req[nu];
    for(size_t j=0; j < p->NS; ++j){
      l[nu]+=((p->gamma[j][nu]*Req[nu]-p->alpha[nu][j])*Seq[j]);
    }
  }
  p->l = l;
  p->m = m;

  return;
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
  nmatrix jac_eq = this->jacobian(dyn_var);
  return jac_eq;
}
ncvector CRModel::eigenvalues_at_equilibrium() const{
  ncvector v;
  nmatrix jac_eq = this->jacobian_at_equilibrium();
  ntype min_element(std::abs(jac_eq[0][0]));

  /* the jacobian should always be a square matrix */
  const unsigned int jacobian_size=jac_eq.size();

  for(size_t i = 0; i < jacobian_size; ++i){
    if(jac_eq[i].size()!=jacobian_size){
      error err("Jacobian is ill formed (not a square matrix).");
      throw err;
    }
    for(size_t j=0; j < jacobian_size; ++j){
      if(jac_eq[i][j]*jac_eq[i][j] > 0. and std::abs(jac_eq[i][j]) < min_element){
        min_element = std::abs(jac_eq[i][j]);
      }
    }
  }

  // for testing purpose
  //min_element = 1.;
  //std::cout << " put min_element as 1" << std::endl;

  Eigen::Matrix<ntype, Eigen::Dynamic, Eigen::Dynamic> jacob;
  jacob.resize(jacobian_size, jacobian_size);
  // we rescale the jacobian such that even the smallest value is of order 1
  for(size_t i=0; i < jacobian_size; ++i){
    for(size_t j=0; j < jacobian_size; ++j){
      jacob(i,j) = jac_eq[i][j]/min_element;
    }
  }
  Eigen::Matrix<nctype, Eigen::Dynamic, 1> eivals = jacob.eigenvalues();
  for(size_t i=0; i < eivals.rows(); ++i){
    v.push_back(eivals(i)*min_element);
    //v.push_back(eivals(i));
  }
  std::sort(v.begin(), v.end(), compare_complex);
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
  os << *metaparameters;
  return os;
}
bool CRModel::constraints_fulfilled(const Metaparameters& m) const{
  if(not(this->positive_parameters())){
    if(m.verbose > 3){
      std::cout << "Model rejected because some of the parameters are not positive" << std::endl;
    }
    return false;
  }

  if(m.energy_constraint and not(this->energy_constraint())){
    if(m.verbose > 3){
      std::cout << "Model rejected because the energy constraint is not fulfilled" << std::endl;
    }
    return false;
  }

  if(not(this->respects_equations_of_evolution_at_equilibrium())){
    if(m.verbose > 3){
      std::cout << "Model rejected because the equations of evolution are not respected at equilibrium " << std::endl;
    }
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
        somme-=p->tau[nu][i];
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
    if(this->metaparameters->verbose>2){
      std::cerr << "\t \t System unfeasible : d contains negative elements : " << p->d << std::endl;
    }
    return false;
  }
  if(not(non_neg_elements(p->l))){
    if(this->metaparameters->verbose>2){
      std::cerr << "\t \t System unfeasible: l contains negative elements : " << p->l << std::endl;
    }
    return false;
  }
  if(not(non_neg_elements(p->m))){
    if(this->metaparameters->verbose>2){
      std::cerr << "\t \t System unfeasible: m contains negative elements : ";
      std::cerr << p->m << std::endl;
    }
    return false;
  }
  return true;
}
bool CRModel::is_dynamically_stable() const{
  return (this->assess_dynamical_stability()==stable);
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
  unsigned int pert_type=this->metaparameters->struct_pert_type;
  if(this->metaparameters->verbose > 1){
    std::cout <<"\t Structurally perturbed the l_mu's the system with parameter delta =" << Delta;
  }
  switch(pert_type){
    case 0:{
      std::uniform_real_distribution<ntype> uniform_distrib(-1., 1.);
      for(size_t mu=0; mu < p->l.size(); ++mu){
        p->l[mu] = p->l[mu]*(1+Delta*uniform_distrib(random_engine));
      }

      if(this->metaparameters->verbose > 1){
        std::cout << " (l0 stays the same)" << std::endl;
      }
      break;
    }

    case 1:{
      std::uniform_real_distribution<ntype> delta_distrib(0, 2.*Delta);
      for(size_t mu=0; mu < p->l.size(); ++mu){
        ntype diminution;
        do{
          diminution=delta_distrib(random_engine);
        }while(diminution > 1);
        p->l[mu]*=(1-diminution);
      }
      if(this->metaparameters->verbose > 1){
        std::cout << " (l0 is now approximately "<< mean(p->l) << ")" << std::endl;
      }
      break;

    }

    default:{
      throw error("Unknown parameter perturbation type, please choose either 0 or 1");

    }
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
double CRModel::get_m0() const{
  Parameter_set* p = this->model_param->get_parameters();
  double m0 = 0.;
  unsigned int NR = p->m.size();
  for(size_t nu = 0; nu < NR; ++nu){
    m0 += p->m[nu]/NR;
  }
  return m0;
}
double CRModel::get_d0() const{
  Parameter_set* p = this->model_param->get_parameters();
  double d0 = 0.;
  unsigned int NS = p->d.size();
  for(size_t i = 0; i < NS; ++i){
    d0 += p->d[i]/NS;
  }
  return d0;
}
nvector CRModel::get_m() const{
  Parameter_set* p = this->model_param->get_parameters();
  return p->m;
}
nvector CRModel::get_d()const{
  Parameter_set* p = this->model_param->get_parameters();
  return p->d;
}
nmatrix CRModel::perturb_abundances(const ntype& delta) const{
  nmatrix* current_eq = &(*eq_vals)[0];
  nmatrix perturb_eq;
  perturb_eq.push_back(nvector());
  perturb_eq.push_back(nvector());

  Parameter_set* p = this->model_param->get_parameters();

  std::uniform_real_distribution<ntype> uniform_distrib(-1., 1.);
  for(size_t i=0; i < p->NR; ++i){
    perturb_eq[0].push_back((*current_eq)[0][i]*(1+delta*uniform_distrib(random_engine)));
  }
  for(size_t i=0; i < p->NS; ++i){
    perturb_eq[1].push_back((*current_eq)[1][i]*(1+delta*uniform_distrib(random_engine)));
  }
  if(this->metaparameters->verbose>1){
    std::cout << "\t Perturbed the resources and consumers abundances with magnitude " << delta << std::endl;
  }
  return perturb_eq;
}
nmatrix CRModel::get_first_equilibrium() const{
  return (*eq_vals)[0];
}
ntype CRModel::get_resilience_jacobian() const{
  /* resilience is defined as 1/l1 where l1 is the largest real part of an eigenvalue of the jacobian at equilibrium */
  ntype resilience = 0.;
  ncvector eigenvals = this->eigenvalues_at_equilibrium();
  ntype largest_real_part = real(eigenvals[0]);
  for(size_t i = 1; i < eigenvals.size(); ++i){
    if(real(eigenvals[i]) > largest_real_part){
      largest_real_part=real(eigenvals[i]);
    }
  }
  resilience = ntype(1./largest_real_part);
  return resilience;
}
ntype CRModel::get_resilience_dynamical_stability(const ntype& delta){
  /* we perturb all the abundances by delta */
  double tend = 0.;
  return ntype(tend);
}
bool CRModel::has_linearly_stable_eq() const{
  using namespace Eigen;
  unsigned int NR = this->metaparameters->NR, NS = this->metaparameters->NS;
  Matrix<ntype, Dynamic, Dynamic> jacob_sym;
  jacob_sym.resize(NR+NS, NR+NS);

  nmatrix jac_eq = this->jacobian_at_equilibrium();
  for(size_t i = 0; i < NR+NS; ++i){
    for(size_t j=0; j < NR+NS; ++j){
      jacob_sym(i,j) = 0.5*(jac_eq[i][j]+jac_eq[j][i]);
    }
  }

  SelfAdjointEigenSolver<Matrix<ntype, Dynamic, Dynamic>> eigensolver(jacob_sym);
  Matrix<ntype, Dynamic,1> eigvals = eigensolver.eigenvalues();
  nvector eigenvalues;
  for(size_t i=0; i < eigvals.rows();++i){
    eigenvalues.push_back(eigvals(i));
  }

  ntype min_eigval = *std::min_element(eigenvalues.begin(), eigenvalues.end());
  ntype max_eigval = *std::max_element(eigenvalues.begin(), eigenvalues.end());

  if(min_eigval > 0.){
    return false;
  }

  if(max_eigval < 0.){
    return true;
  }

  std::cout << "Could not determine whether or not the system was stable, returning false to make sure" << std::endl;
  return false;
}
systemstability CRModel::assess_dynamical_stability() const{
  ncvector eigvals = this->eigenvalues_at_equilibrium();
  ntype max_real_eigval = real(eigvals[0]);
  for(size_t i=1; i < eigvals.size(); ++i){
    ntype test = real(eigvals[i]);
    if(test > max_real_eigval){
      max_real_eigval = test;
    }
  }

  if(max_real_eigval > EIGENSOLVER_PRECISION){
    if(this->metaparameters->verbose>1){
      std::cout << "\t The system is dynamically unstable" << std::endl;
    }
    return systemstability(unstable);
  }else if(abs(max_real_eigval) <= EIGENSOLVER_PRECISION){
    if(this->metaparameters->verbose>1){
      std::cout << "\t The system is dynamically marginally stable" << std::endl;
    }
    return systemstability(marginal);
  }else{
    if(this->metaparameters->verbose>1){
      std::cout << "\t The system is dynamically stable" << std::endl;
    }
    return systemstability(stable);
  }
}
nvector CRModel::get_resources_equilibrium(unsigned int n) const {
  return (*eq_vals)[n][0];
}
nvector CRModel::get_consumers_equilibrium(unsigned int n)const {
  return (*eq_vals)[n][1];
}
ntype CRModel::environmental_flux_resource(unsigned int mu) const {
  return this->model_param->get_parameters()->l[mu];
}
ntype CRModel::environmental_flux_equilibrium_resource (unsigned int mu, unsigned int equilibrium_number) const {
  return this->environmental_flux_resource(mu);
}
ntype CRModel::diffusion_flux_resource(unsigned int mu, const nvector& R) const {
  return -this->model_param->get_parameters()->m[mu]*R[mu];
}
ntype CRModel::diffusion_flux_equilibrium_resource(unsigned int mu, unsigned int equilibrium_number) const {
  return this->diffusion_flux_resource(mu, get_resources_equilibrium(equilibrium_number));
}
ntype CRModel::syntrophy_flux_resource(unsigned int mu, const nvector& R, const nvector& S)const {
  Parameter_set * p = this->model_param->get_parameters();
  ntype flux=0.;
  for(size_t j=0; j < S.size(); ++j){
    flux+=p->alpha[mu][j]*S[j];
  }
  return flux;
}
ntype CRModel::syntrophy_flux_equilibrium_resource(unsigned int mu, unsigned int equilibrium_number) const {
  return this->syntrophy_flux_resource(mu, this->get_resources_equilibrium(equilibrium_number), this->get_consumers_equilibrium(equilibrium_number));
}
ntype CRModel::consumption_flux_resource(unsigned int mu, const nvector& R, const nvector& S) const {
  Parameter_set * p = this->model_param->get_parameters();
  ntype flux=0.;
  for(size_t j=0; j < S.size(); ++j){
    flux-=p->gamma[j][mu]*R[mu]*S[j];
  }
  return flux;
}
ntype CRModel::consumption_flux_equilibrium_resource(unsigned int mu, unsigned int equilibrium_number) const {
  return this->consumption_flux_resource(mu, this->get_resources_equilibrium(equilibrium_number), this->get_consumers_equilibrium(equilibrium_number));
}
ntype CRModel::consumption_intake_flux_consumer(unsigned int i, const nvector&R, const nvector& S) const {
  Parameter_set* p=this->model_param->get_parameters();
  ntype flux=0.;
  for(size_t mu=0; mu < R.size();++mu){
    flux+=p->sigma[i][mu]*p->gamma[i][mu]*S[i]*R[mu];
  }
  return flux;
}
ntype CRModel::consumption_intake_flux_equilibrium_consumer(unsigned int i, unsigned int equilibrium_number) const {
  return this->consumption_intake_flux_consumer(i, this->get_resources_equilibrium(equilibrium_number), this->get_consumers_equilibrium(equilibrium_number));
}
ntype CRModel::diffusion_flux_consumer(unsigned int i, const nvector& S) const {
  return -this->model_param->get_parameters()->d[i]*S[i];
}
ntype CRModel::diffusion_flux_equilibrium_consumer(unsigned int i, unsigned int eq_number) const {
  return this->diffusion_flux_consumer(i, this->get_consumers_equilibrium(eq_number));
}
ntype CRModel::syntrophy_flux_consumer(unsigned int i, const nvector& R, const nvector& S) const {
  ntype flux=0.;
  Parameter_set* p = this->model_param->get_parameters();
  for(size_t mu=0; mu < R.size(); ++mu){
    flux-=p->alpha[mu][i]*S[i];
  }
  return flux;
}
ntype CRModel::syntrophy_flux_equilibrium_consumer(unsigned int i, unsigned int eq_number) const {
  return this->syntrophy_flux_consumer(i,this->get_resources_equilibrium(eq_number),this->get_consumers_equilibrium(eq_number));
}
ntype CRModel::order_parameter() const{
  ntype syntrophy_flux_consumers = 0.;
  ntype consumption_flux_consumers=0.;
  ntype diffusion_flux_consumers=0.;
  for(size_t i=0; i < this->metaparameters->NS; ++i){
    syntrophy_flux_consumers+=this->syntrophy_flux_equilibrium_consumer(i);
    consumption_flux_consumers+=this->consumption_intake_flux_equilibrium_consumer(i);
    diffusion_flux_consumers+=this->diffusion_flux_equilibrium_consumer(i);
  }
  ntype environmental_flux=0.;
  ntype diffusion_flux_resources=0.;
  for(size_t mu=0; mu < this->metaparameters->NR;++mu){
    environmental_flux+=this->environmental_flux_equilibrium_resource(mu);
    diffusion_flux_resources+=this->diffusion_flux_equilibrium_resource(mu);
  }

  return environmental_flux/(diffusion_flux_resources+diffusion_flux_consumers);
}
Metaparameters* CRModel::get_metaparameters() const{
  return this->metaparameters;
}
Parameter_set* CRModel::get_parameter_set() const{
  return this->model_param->get_parameters();
}
Model_parameters* CRModel::get_model_parameters() const{
  return this->model_param;
}
ntensor* CRModel::get_equilibrium_abundances() const{
  return this->eq_vals;
}
func_equ_evol CRModel::get_equations_of_evolution() const{
  return this->equations_of_evolution;
}
bool CRModel::respects_equations_of_evolution_at_equilibrium() const{
  Parameter_set* p=this->get_parameter_set();

  /* we check that for each equilibrium found, the equations of evolution are fulfilled */
  for(size_t i=0; i < this->get_equilibrium_abundances()->size();++i){
    double y[p->NR+p->NS];
    double f[p->NR+p->NS];

    nvector R = this->get_resources_equilibrium(i);
    nvector S = this->get_consumers_equilibrium(i);
    for(size_t nu=0; nu < p->NR;++nu){
      y[nu]=R[nu];
    }
    for(size_t j=0; j < p->NS;++j){
      y[p->NR+j]=S[j];
    }

    this->equations_of_evolution(0, y, f, p);

    for(size_t j=0; j < p->NS+p->NR;++j){
      if(abs(f[j])>ZERO){
        if(this->metaparameters->verbose>3){
          std::cout << "System rejected because element " << j << " of system not fulfilling equations of evolution ("<<abs(f[j])<<")" << std::endl;
        }
        return false;
      }
    }
  }
  return true;
}

nmatrix CRModel::get_Beta_matrix(unsigned int n) const{
  Parameter_set* p= this->get_parameter_set();
  nmatrix Beta(p->NS, nvector(p->NR, 0.));
  for(size_t i=0; i < p->NS; ++i){
    for(size_t nu=0; nu < p->NR; ++nu){
      Beta[i][nu]=(p->sigma)[i][nu]*(p->gamma[i][nu])*(this->get_consumers_equilibrium(n))[i];
    }
  }
  return Beta;
}
nmatrix CRModel::get_Gamma_matrix(unsigned int n) const{
  Parameter_set* p= this->get_parameter_set();
  nmatrix Gamma(p->NR, nvector(p->NS, 0.));
  for(size_t mu=0; mu < p->NR; ++mu){
    for(size_t j=0; j < p->NS; ++j){
      Gamma[mu][j]= -(p->gamma)[j][mu]*(this->get_resources_equilibrium(n))[mu]+(p->alpha)[mu][j];
    }
  }
  return Gamma;
}

nvector CRModel::get_Delta_vector(unsigned int n) const{
  Parameter_set* p=this->get_parameter_set();
  nvector Delta(p->NR, 0.);
  for(size_t mu=0; mu < p->NR; ++mu){
    Delta[mu]=p->m[mu];
    for(size_t j=0; j < p->NS; ++j){
      Delta[mu]+=p->gamma[j][mu]*(this->get_consumers_equilibrium(n))[j];
    }
  }
  return Delta;
}

nctype CRModel::largest_eigenvalue_at_equilibrium() const{
  ncvector eigvals = this->eigenvalues_at_equilibrium();
  return eigvals[eigvals.size()-1];
}

ntype CRModel::critical_radius() const{
  nmatrix Gamma=this->get_Gamma_matrix();
  nmatrix Beta=this->get_Beta_matrix();
  nvector Delta=this->get_Delta_vector();

  ntype max_Gamma=0.;
  ntype max_Beta=0.;

  for(size_t i=0; i < this->metaparameters->NS; ++i){
    ntype local_Beta=0.;
    for(size_t nu=0; nu < this->metaparameters->NR; ++nu){
      local_Beta+=abs(Beta[i][nu]);
    }
    if(local_Beta>max_Beta){
      max_Beta=local_Beta;
    }
  }

  for(size_t mu=0; mu < this->metaparameters->NR; ++mu){
    ntype local_Gamma=Delta[mu];
    for(size_t j=0; j < this->metaparameters->NS; ++j){
      local_Gamma+=abs(Gamma[mu][j]);
    }
    if(local_Gamma>max_Gamma){
      max_Gamma=local_Gamma;
    }
  }

  if(max_Gamma > max_Beta){
    return max_Gamma;
  }
  return max_Beta;

}


bool CRModel::is_in_weak_LRI() const{
  nmatrix Gamma=this->get_Gamma_matrix();
  nmatrix Beta=this->get_Beta_matrix();
  nmatrix GammaBeta=Gamma*Beta;

  for(size_t mu=0; mu < GammaBeta.size(); ++mu){
    ntype abs_outer_diag=0.;
    for(size_t nu=0; nu < GammaBeta.size(); ++nu){
      if(mu!=nu){
        abs_outer_diag+=abs(GammaBeta[mu][nu]);
      }
    }
    if(GammaBeta[mu][mu]>=-abs_outer_diag){
      if(this->metaparameters->verbose>1){
        std::cout << "\t The system is not in the weak LRI regime." << std::endl;
      }
      return false;
    }
  }
  if(this->metaparameters->verbose>1){
    std::cout << "\t The system is in the weak LRI regime." << std::endl;
  }
  return true;
}

ntype CRModel::max_perturbation_dynamically_stable(ntype accuracy) const{
  ntype delta=0.;
  Extinction new_equilib;
  do{
    delta+=accuracy;
    new_equilib = this->evolve_until_equilibrium_from_abundances(this->perturb_abundances(delta), this->metaparameters->convergence_threshold);
  }while(new_equilib.extinct<1.);
  return delta;
}

bool CRModel::is_in_strong_LRI() const{
  nmatrix Gamma=this->get_Gamma_matrix();
  nmatrix Beta=this->get_Beta_matrix();
  nmatrix GammaBeta=Gamma*Beta;

  for(size_t mu=0; mu < GammaBeta.size(); ++mu){
    ntype abs_outer_diag=0.;
    for(size_t nu=0; nu < GammaBeta.size(); ++nu){
      if(mu!=nu){
        abs_outer_diag+=abs(GammaBeta[mu][nu]);
      }
    }
    if(GammaBeta[mu][mu]>=-abs_outer_diag-pow(this->critical_radius(),2.)){
      if(this->metaparameters->verbose>1){
        std::cout << "\t The system is not in the strong LRI regime." << std::endl;
      }
      return false;
    }
  }
  if(this->metaparameters->verbose>1){
    std::cout << "\t The system is in the strong LRI regime." << std::endl;
  }
  return true;
}

bool CRModel::is_feasible() const{
  Metaparameters* m = this->metaparameters;
  if(not(this->positive_parameters())){
    if(m->verbose > 3){
      std::cout << "Model rejected because some of the parameters are not positive" << std::endl;
    }
    return false;
  }

  if(m->energy_constraint and not(this->energy_constraint())){
    if(m->verbose > 3){
      std::cout << "Model rejected because the energy constraint is not fulfilled" << std::endl;
    }
    return false;
  }

  if(not(this->respects_equations_of_evolution_at_equilibrium())){
    if(m->verbose > 3){
      std::cout << "Model rejected because the equations of evolution are not respected at equilibrium " << std::endl;
    }
    return false;
  }
  return true;
}

nmatrix CRModel::get_biomass_flux_network()const{
  unsigned int NS = this->metaparameters->NS;
  unsigned int NR = this->metaparameters->NR;

  nmatrix gamma = this->get_parameter_set()->gamma;
  nmatrix alpha = this->get_parameter_set()->alpha;

  nmatrix f_network(NS+NR, nvector(NS+NR, 0.));

  // the only non-zero blocks are the outer diagonal blocks
  for(size_t mu=0; mu < NR; ++mu){
    for(size_t i=0; i < NS; ++i){
      if(gamma[i][mu]>0){
          f_network[mu][i+NR] = 1.;
      }
      if(alpha[mu][i]>0){
        f_network[i+NR][mu] = 1.;
      }
    }
  }

  return f_network;
}
