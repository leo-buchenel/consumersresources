#include <iostream>
#include <fstream>
#include "CRModel.h"

using namespace std;

/*  This script quantifies how many feasible systems you build for certain parameters. */
int main(int argc, char * argv[]){
  try{
    Metaparameters metaparams(argc, argv);
    std::vector<std::string> matrix_list = load_food_matrix_list(metaparams.foodmatrixpath);
    std::string syntrophy_folder = metaparams.syntrophy_matrix_path;

    ntype min_gamma0=0.01, max_gamma0=1.;
    ntype min_S0=0.01, max_S0=1.;
    unsigned int Nsimuls=200;
    unsigned int Npoints=100;

    nvector gamma0_interval=linear_interval(min_gamma0, max_gamma0, Npoints);
    nvector S0_interval=linear_interval(min_S0, max_S0, Npoints);

    std::ofstream myfile = open_external_file_append(metaparams.save_path);
    for(auto mat: matrix_list){
      metaparams.foodmatrixpath=mat;
      metaparams.syntrophy_matrix_path=optimal_alpha_matrix_path_from_syntrophy_folder(metaparams);
      myfile << metaparams.foodmatrixpath << " ";
      for(auto g : gamma0_interval){
        for(auto S : S0_interval){
          metaparams.gamma0=g;
          metaparams.S0=S;
          ntype feasability_proba = find_feasability_probability(metaparams, Nsimuls);
          myfile << g << " " << S << " " << feasability_proba << " ";
        }
      }
      metaparams.syntrophy_matrix_path=syntrophy_folder;
      myfile << std::endl;
    }

    myfile.close();

  }catch(error e){
    e.handle();
  }
  return 0;
}
