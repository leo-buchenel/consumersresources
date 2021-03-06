#include <iostream>
#include "CRModel.h"

using namespace std;

int main(int argc, char * argv[]){
  try{
    Metaparameters metaparams(argc, argv);
    initialize_random_engine(metaparams);
    double int_start = 0., int_end = 1.;
    unsigned int N = 100;
    nvector Delta_interval;
    for(size_t i = 0; i < N; ++i){
      Delta_interval.push_back(int_start+i*(int_end-int_start)/(N-1));
    }

    write_prob_greater_than_one_delta_interval(&metaparams, Delta_interval, 1000);
  }catch(error e){
    e.handle();
  }

  return 0;
}
