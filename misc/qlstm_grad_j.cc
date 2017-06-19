#include "out/qlstm_grad_j.c"
#include "misc/qlstm_grad_common.h"

Tensor* qlstm_grad_i() {
  Tensor* toi;
  Tensor* toj;
  Tensor* tof;
  Tensor* too;
  Tensor* toc;
  qlstm_grad(&toi, &toj, &tof, &too, &toc);
  return toj;
}

int main(int argc, const char* argv[]) {
  init();
  run(argc, argv, qlstm_grad_i);
}
