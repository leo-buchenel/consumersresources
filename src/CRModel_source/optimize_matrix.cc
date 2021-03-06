#include "../../include/CRModel.h"


nmatrix optimal_syntrophy_from_consumption(const nmatrix& gamma, bool coprophagy, MonteCarloSolver& mcs){
  return optimal_syntrophy_from_consumption(gamma, coprophagy, mcs, connectance(gamma));
}

nmatrix optimal_syntrophy_from_consumption(const nmatrix& gamma, bool coprophagy, MonteCarloSolver& mcs, const ntype& target_conn){
  nmatrix unit_gamma(gamma.size(), nvector(gamma[0].size(), 0.));
  for(size_t i=0; i < gamma.size(); ++i){
    for(size_t mu=0; mu < gamma[0].size(); ++mu){
      if(gamma[i][mu]>0){
        unit_gamma[i][mu]=1.;
      }
    }
  }
  nmatrix alpha = create_alpha(target_conn, unit_gamma, coprophagy);
  apply_MC_algorithm(alpha, unit_gamma, coprophagy, mcs);
  return alpha;
}

ntype quadratic_form_Alberto(const nmatrix& alpha, const nmatrix& gamma, void* params){
  const nvector& u = *(nvector*)(params);
  return u*(alpha*gamma)*u;
}

// this function is the same as "quadratic_form" except corrected according to Alberto's ideas we discussed in May 2021
ntype quadratic_form_corrected_AlbertoMay2021(const nmatrix& alpha, const nmatrix& gamma, void* params){
  unsigned int NR=alpha.size();
  nmatrix alpha_gamma=alpha*gamma;
  nmatrix gamma_square=transpose(gamma)*gamma;

  ntype to_minimize=0.;
  /* we want the absolute trace to be as close to zero as possible*/
  /* and we want the rest to be as close to zero as possible*/
  ntype off_diag=0., trace=0.;
  for(size_t mu=0; mu < NR;++mu){
    trace+=alpha_gamma[mu][mu]-gamma_square[mu][mu];
    for(size_t nu=0; nu < NR;++nu){
      if(nu!=mu){
        off_diag+=abs(alpha_gamma[mu][nu]-gamma_square[mu][nu]);
      }
    }
  }
  to_minimize=trace+off_diag;
  return to_minimize;
}

ntype quadratic_form(const nmatrix& alpha, const nmatrix& gamma, void* params){
  /* the goal is to minimize the maximal sum of LHS in the intra resource regime */
  unsigned int NR=alpha.size();
  nmatrix alpha_gamma=alpha*gamma;
  nmatrix gamma_square=transpose(gamma)*gamma;

  ntype to_minimize=0.;
  /* we want the absolute trace to be as close to zero as possible*/
  /* and we want the rest to be as close to zero as possible*/
  ntype off_diag=0., trace=0.;
  for(size_t mu=0; mu < NR;++mu){
    trace+=abs(alpha_gamma[mu][mu]);
    for(size_t nu=0; nu < NR;++nu){
      if(nu!=mu){
        off_diag+=abs(alpha_gamma[mu][nu]-gamma_square[mu][nu]);
      }
    }
  }
  to_minimize=trace+off_diag;
  return to_minimize;
}
ntype probability_density(const nmatrix& alpha, const nmatrix& gamma, const MonteCarloSolver& mcs){
  return exp(-mcs.cost_function(alpha, gamma, mcs.additional_params)/mcs.T);
}
nmatrix proposed_new_alpha(const nmatrix & alpha, const nmatrix& gamma, bool coprophagy, unsigned int steps){
  return proposed_new_alpha_Alberto(alpha, gamma, coprophagy,steps);
  //return flip_one_element(alpha, gamma, coprophagy);
}

/* creates an optimal consumption matrix with connectance ctarg  */
nmatrix optimal_consumption_matrix(unsigned int NR, unsigned int NS, const ntype& ctarg, MonteCarloSolver& mcs){
  nmatrix gamma = create_gamma(NR, NS, ctarg);
  nmatrix dummy(NS, nvector(NR, 0.));
  apply_MC_algorithm(gamma, dummy, true, mcs);
  return gamma;
}

nmatrix create_gamma(unsigned int NR, unsigned int NS, const ntype& ctarg){
  nmatrix gamma(NS, nvector(NR,0.));
  std::uniform_real_distribution<ntype> unif_distrib(0., 1.);
  /*  we fill gamma such that its connectance is ctarg while making sure that
      every species eats something and every resource is eaten by one species */
  unsigned int number_of_links = ctarg*NR*NS;
  unsigned int diag_elements = std::min(NR, NS);

  if(diag_elements > number_of_links){
    throw error("The target connectance is too low.");
  }

  if(diag_elements==number_of_links){
    if(has_an_empty_row(gamma)|| has_an_empty_column(gamma)){
      throw error("Please increase connectance, filling up the diagonal took all available links.");
    }
  }

  /* we fill up the diagonal elements first, in the case NR=NS this ensures that no column or row is empty */
  for(size_t k=0; k < diag_elements; ++k){
    gamma[k][k]=1;
  }

  unsigned int to_fill = number_of_links-diag_elements;
  ntype proba = ntype(to_fill)/ntype(NR*NS);

  /* finally we fill the remaining links */
  do{
    for(size_t i=0; i < NS; ++i){
      for(size_t mu=0; mu < NR ;++mu){
        if(i!=mu){
          gamma[i][mu]=0.;
          if(unif_distrib(random_engine) < proba){
            gamma[i][mu]=1.;
          }
        }
      }
    }

  }while(has_an_empty_row(gamma)|| has_an_empty_column(gamma));
  std::cout << "Created gamma with connectance " << connectance(gamma) << std::endl;
  return gamma;
}
nmatrix flip_one_element(const nmatrix& alpha, const nmatrix& gamma, bool allowed_coprophagy){
  unsigned int row_index, col_index;
  nmatrix new_alpha;

  std::uniform_int_distribution<unsigned int> row_dist(0, alpha.size()-1);
  std::uniform_int_distribution<unsigned int> col_dist(0, alpha[0].size()-1);
  do{
    /* set new_alpha to alpha so that in any case, new_alpha and alpha differ by one element */
    new_alpha=alpha;
    /* pick random element and flip it */
    row_index=row_dist(random_engine);
    col_index=col_dist(random_engine);

    new_alpha[row_index][col_index]=1-alpha[row_index][col_index];

  }while(not(allowed_coprophagy) && is_there_coprophagy(new_alpha, gamma));

  return new_alpha;
}
bool choose_next_matrix(nmatrix& alpha, const nmatrix& gamma, bool coprophagy, unsigned int steps, unsigned int& fails, const MonteCarloSolver& mcs){
  nmatrix new_alpha=proposed_new_alpha(alpha, gamma, coprophagy, steps);
  ntype proba_ratio=probability_density(new_alpha, gamma, mcs)/probability_density(alpha, gamma, mcs);
  if(proba_ratio>1){
    alpha=new_alpha;
    fails=0;
    return true;
  }else{
    fails+=1;
    std::uniform_real_distribution<ntype> real_distrib(0., 1.);
    if(real_distrib(random_engine)<proba_ratio){
      alpha=new_alpha;
      return true;
    }
  }
  return false;
}
void apply_MC_algorithm(nmatrix& alpha, const nmatrix& gamma, bool coprophagy, MonteCarloSolver& mcs){
  bool stop=false;
  unsigned int steps=0;
  unsigned int fails=0;
  bool changed=false;
  bool reached_zero=false;
  bool max_steps_reached=false;
  nvector last_changed_elements, average_cost_function;
  /* idea of this variable, if the cost function hasn't decreased over that number of steps, we stop the simulations */
  unsigned int Naverage=100000;
  bool av_cost_function_converging=false;
  while(!stop){
    changed=choose_next_matrix(alpha, gamma, coprophagy, steps, fails, mcs);
    reached_zero=(mcs.cost_function(alpha,gamma, mcs.additional_params)<1e-15);


    if(changed){
      fails=0;
      last_changed_elements.push_back(mcs.cost_function(alpha,gamma, mcs.additional_params));
    }else{
      fails+=1;
    }

    if(last_changed_elements.size() > Naverage){
      last_changed_elements.erase(last_changed_elements.begin());
    }

    if(last_changed_elements.size() == Naverage && steps%Naverage==0){
      average_cost_function.push_back(mean(last_changed_elements));
    }


    /* if the average cost hasn't decreased significantly, we stop the simulation */
    unsigned int size_cf = average_cost_function.size();
    if(size_cf > 2){
      av_cost_function_converging = !(average_cost_function[size_cf-1] < 0.99*average_cost_function[size_cf-2]);
    }else{
      av_cost_function_converging=false;
    }
    /* when the move has not been accepted too many times, increase temp */
    if(fails>=mcs.max_fails){
      mcs.T/=mcs.annealing_const;
    }

    /* at a given frequency, the temperature is reduced */
    if(steps%mcs.annealing_freq==0){
      mcs.T*=mcs.annealing_const;
    }

    max_steps_reached = (steps>=mcs.max_steps);

    // removed the reached_zero condition because we are now considering energies which may be negative
    //stop = max_steps_reached || reached_zero || av_cost_function_converging;
    stop = max_steps_reached || av_cost_function_converging;
    

    if(steps%mcs.display_stride==0 || stop){
      std::cout << "\t Step " << steps;
      std::cout << ", T=" << mcs.T;
      std::cout <<", cost function=" << mcs.cost_function(alpha,gamma, mcs.additional_params) ;
      std::cout << ", nestedness=" << nestedness(alpha);
      std::cout << ", connectance=" << connectance(alpha);
      if(stop){
        std::cout << std::endl << "-------" << std::endl << "STOPPING THE ALGORITHM ";
      }
      /* not displaying this message anymore since consider also negative cost functions
      if(reached_zero){
        std::cout << " -> reached zero on the cost function." << std::endl;
      }
      */
      if(max_steps_reached){
        std::cout << " -> max number of steps reached." << std::endl;
      }

      if(av_cost_function_converging){
        std::cout << " -> cost function only fluctuating around its average value." << std::endl;
      }
     // std::cout << ", matrix = " << std::endl;
      //display_food_matrix(std::cout, alpha);
      std::cout << std::endl;
    }
    steps+=1;
  }
  return;
}
nmatrix create_alpha(const ntype& connectance_in, const nmatrix& gamma, bool allowed_coprophagy){
  nmatrix alpha(gamma[0].size(),nvector(gamma.size(), 0));
  std::uniform_real_distribution<ntype> real_distrib(0., 1.);
  do{
    for(size_t mu=0; mu < alpha.size(); ++mu){
      for(size_t i=0; i < alpha[mu].size();++i){
        if(real_distrib(random_engine)<connectance_in){
          alpha[mu][i]=1;
        }
      }
    }
  }while(not(allowed_coprophagy) && is_there_coprophagy(alpha, gamma));
  std::cout << "Created initial alpha guess with nestedness " << nestedness(alpha) << std::endl;
  return alpha;
}
nmatrix proposed_new_alpha_Alberto(const nmatrix& alpha, const nmatrix& gamma, bool coprophagy, unsigned int steps){
  nmatrix new_alpha=alpha;
  if(steps%2==0){
    modify_row(new_alpha, gamma, coprophagy);
  }else{
    modify_column(new_alpha, gamma, coprophagy);
  }
  return new_alpha;
}
void modify_row(nmatrix& alpha, const nmatrix& gamma, bool coprophagy){
  /*  we first choose a random row (i.e. resource) that is neither completely empty nor
      completely filled */
  const unsigned int NR = alpha.size();
  const unsigned int NS = alpha[0].size();
  std::vector<unsigned int> sum_row(NR, 0.);


  for(size_t nu=0; nu < NR; ++nu){
    for(size_t i=0; i < NS; ++i){
      sum_row[nu]+=(unsigned int)(alpha[nu][i]);
    }
  }

  std::uniform_int_distribution<size_t> random_resources(0, NR-1);

  unsigned int mu=random_resources(random_engine);

  while(sum_row[mu]==0 || sum_row[mu]==NS){
    mu=random_resources(random_engine);
  }

  /*  then we find the indices of the columns (i.e. consumers) in that row
      (i.e. resource) which are zeros and which are ones */
  std::vector<size_t> zero_els;
  std::vector<size_t> one_els;

  for(size_t i=0; i < NS; ++i){
    if(alpha[mu][i]<1e-15){
      zero_els.push_back(i);
    }else if(alpha[mu][i]==1){
      one_els.push_back(i);
    }
  }

  /*  we choose one index which has one and another which has zero, may have a problem here :
      we did not explicitly check that zero_els.size() was at least 1 !! could have the setting where
      zero_els is empty and one_els is full or vice-versa */
  if(zero_els.size()==0){
    std::cout << "NR = " << NR << ", NS=" << NS << std::endl;
    std::cout << "Picked row " << mu << std::endl;
    std::cout << "The zero elements are " << zero_els << std::endl;
    throw error("zero_els has size 0 (row) ", 1);
  }

  if(one_els.size()==0){
    std::cout << "NR = " << NR << ", NS=" << NS << std::endl;
    std::cout << "Picked row " << mu << std::endl;
    std::cout << "The one elements are " << one_els << std::endl;
    throw error("one_els has size 0 (row) ", 1);
  }
  std::uniform_int_distribution<size_t> zero_els_indices(0, zero_els.size()-1);
  std::uniform_int_distribution<size_t> one_els_indices(0, one_els.size()-1);

  size_t zero_index=zero_els[zero_els_indices(random_engine)];
  size_t one_index=one_els[one_els_indices(random_engine)];

  if(one_index>=NS){
    display_food_matrix(std::cout, alpha);
    std::cout << "NR = " << NR << ", NS=" << NS << std::endl;
    std::cout << "one_index=" << one_index << std::endl;
    throw error("one_index has an index larger than it should have (row)",1);
  }

  if(zero_index>=NS){
    display_food_matrix(std::cout, alpha);
    std::cout << "NR = " << NR << ", NS=" << NS << std::endl;
    std::cout << "zero_index=" << zero_index << std::endl;
    throw error("zero_index has an index larger than it should have (row)",1);
  }

  /*  we check if there is another non empty value in the column (i.e.
      check if that species releases to something else) */
  bool conditionRel=false;
  for(size_t nu=0; nu < NR && !conditionRel; ++nu){
    if(mu!=nu){
      if(alpha[nu][one_index]==1){
        conditionRel=true;
      }
    }
  }

  /* we check if with the chosen zero index there is coprophagy */
  bool conditionCopr=false;
  if(gamma[zero_index][mu]!=1){
    conditionCopr=true;
  }else if(coprophagy){
    conditionCopr=true;
  }

  /* if both conditions are fulfilled, we can swap the two elements */
  if(conditionCopr && conditionRel){
    alpha[mu][zero_index]=1;
    alpha[mu][one_index]=0;
  }
  return;
}
void modify_column(nmatrix& alpha, const nmatrix& gamma, bool coprophagy){
  /*  we first choose a random column (i.e consumer) that is not empty and
      not completely filled */
  const unsigned int NR = alpha.size();
  const unsigned int NS = alpha[0].size();
  std::vector<unsigned int> sum_column(NS, 0.);
  for(size_t mu=0; mu < NR; ++mu){
    for(size_t i=0; i < NS; ++i){
      sum_column[i]+=(unsigned int)(alpha[mu][i]);
    }
  }

  std::uniform_int_distribution<size_t> random_consumers(0, NS-1);
  unsigned int k=random_consumers(random_engine);
  while(sum_column[k]==0 || sum_column[k]==NR){
    k=random_consumers(random_engine);
  }

  /*  then we find the indices of the rows in that column which are zeros
      or which are one */
  std::vector<size_t> zero_els;
  std::vector<size_t> one_els;
  for(size_t mu=0; mu < NR; ++mu){
    if(alpha[mu][k]<1e-15){
      zero_els.push_back(mu);
    }else if(alpha[mu][k]==1){
      one_els.push_back(mu);
    }
  }

  if(zero_els.size()==0){
    std::ofstream myfile=open_external_file_truncate("data_output/error_matrix.out");
    std::cout << std::endl;
    display_food_matrix(myfile, alpha);
    myfile.close();
    display_food_matrix(std::cout, alpha);
    std::cout << std::endl;
    std::cout << "NR = " << NR << ", NS=" << NS << std::endl;
    std::cout << "Picked column " << k << std::endl;
    std::cout << "The zero elements are " << zero_els << std::endl;
    throw error("zero_els has size 0 (column)", 1);
  }

  if(one_els.size()==0){
    std::cout << std::endl;
    display_food_matrix(std::cout, alpha);
    std::cout << std::endl;
    std::cout << "NR = " << NR << ", NS=" << NS << std::endl;
    std::cout << "Picked column " << k << std::endl;
    std::cout << "The one elements are " << one_els << std::endl;
    throw error("one_els has size 0 (column)", 1);
  }

  /*  we then choose randomly one of the zero elements and one of the one elements */
  std::uniform_int_distribution<size_t> zero_els_indices(0, zero_els.size()-1);
  std::uniform_int_distribution<size_t> one_els_indices(0, one_els.size()-1);

  size_t zero_index=zero_els[zero_els_indices(random_engine)];
  size_t one_index=one_els[one_els_indices(random_engine)];

  if(one_index>=NR){
    std::cout << std::endl;
    display_food_matrix(std::cout, alpha);
    std::cout << "one_index=" << one_index << std::endl;
    throw error("one_index has an index larger than it should have (column)",1);
  }

  if(zero_index>=NR){
    std::cout << std::endl;
    display_food_matrix(std::cout, alpha);
    std::cout << "zero_index=" << zero_index << std::endl;
    throw error("zero_index has an index larger than it should have (column)",1);
  }

  /*  we check if there is another non empty value in the row with the one, i.e.
      if the resource is being released by another species */
  bool conditionRel=false;
  for(size_t i=0; i < NS && !conditionRel; ++i){
    if(i!=k){
      if(alpha[one_index][i]==1){
        conditionRel=true;
      }
    }
  }

  /* we check if with the chosen zero index there is coprophagy */
  bool conditionCopr=false;
  if(gamma[k][zero_index]!=1){
    conditionCopr=true;
  }else if(coprophagy){
    conditionCopr=true;
  }

  /* if both conditions are fulfilled we can swap the two elements */
  if(conditionCopr && conditionRel){
    alpha[zero_index][k]=1;
    alpha[one_index][k]=0;
  }

  return;
}

ntype quadratic_form_low_intra_resource_interaction(const nmatrix& alpha, const nmatrix& gamma, void* params){
  Metaparameters* m = (Metaparameters*)(params);
  return m->quadratic_form_low_intra_resource_interaction(alpha, gamma);
}
ntype quadratic_form_nestedness(const nmatrix& gamma, const nmatrix& dummy, void*params){
  ntype* target = (ntype*)(params);
  return abs(nestedness(gamma)-(*target));
}
ntype quadratic_form_nestedness_rank(const nmatrix& gamma, const nmatrix& dummy, void*params){
  int max_rank = gamma.size();
  if(gamma[0].size() < max_rank){
    max_rank =gamma[0].size();
  }
  return quadratic_form_nestedness(gamma, dummy, params)+max_rank-rank(gamma);
}

ntype quadratic_form_LRI_with_critical_radius(const nmatrix& alpha, const nmatrix& gamma, void* params){
  Metaparameters* m= (Metaparameters*)(params);
  return m->accurate_quadratic_form_LRI(alpha, gamma);
}
ntype quadratic_form_LRI_newly_corrected(const nmatrix& alpha, const nmatrix& gamma, void* params){
  Metaparameters* m= (Metaparameters*)(params);
  return m->newly_corrected_quadratic_form_LRI(alpha, gamma);
}
