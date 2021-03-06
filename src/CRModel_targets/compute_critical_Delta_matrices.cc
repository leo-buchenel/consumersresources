#include <iostream>
#include "CRModel.h"
#include <fstream>
using namespace std;

int main(int argc, char * argv[]){
  try{

    Metaparameters metaparams(argc, argv);
    initialize_random_engine(metaparams);
    std::vector<std::string> matrices_path=load_food_matrix_list(metaparams.foodmatrixpath);
    std::string syntrophy_folder=metaparams.syntrophy_matrix_path;


    std::ofstream myfile;
    myfile.open(metaparams.save_path, std::ios::app);
    if(metaparams.verbose > 0){
      std::cout << "The critical deltas for the following matrices will be computed :";
      for(size_t i = 0 ; i < matrices_path.size(); ++i){
        std::cout << std::endl << matrices_path[i];
      }
      std::cout << std::endl;
    }
    bool save_success(false);
    if(not(myfile.is_open())){
      std::cerr << "Could not open " << metaparams.save_path << " to write the new equilibrium of the system" << std::endl;
    }else{
      if(metaparams.verbose > 0){
        std::cout << "Successfully opened " << metaparams.save_path <<", attempting now to find the critical delta of every listed matrix " << std::endl;
      }
      save_success = true;
      for(size_t i = 0; i < matrices_path.size();++i){
          metaparams.foodmatrixpath = matrices_path[i];
          metaparams.syntrophy_matrix_path=optimal_alpha_matrix_path_from_syntrophy_folder(metaparams);
          delta_solver solv_params = {fitmode(sigmoidal),eqmode(oneextinct), stabilitymode(structural)};

          std::time_t start, end;
          std::time(&start);
          statistics delta = compute_critical_Delta(metaparams, solv_params);
          std::time(&end);
          std::cout << "Computed critical delta for " << matrices_path[i];
          double time_taken = double(end-start);
          std::cout << " in " << time_taken << " seconds " << std::endl;
          std::cout << "The relative error achieved with those parameters is " <<  delta.std_deviation_/delta.mean_*100. << "%" << std::endl;
          myfile << matrices_path[i] << " " << delta.mean_ << " " << delta.std_deviation_ << std::endl;
          metaparams.syntrophy_matrix_path=syntrophy_folder;
      }
    }
    myfile.close();
  }catch(error e){
    e.handle();
  }

  return 0;
}
