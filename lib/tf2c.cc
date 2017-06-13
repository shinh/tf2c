#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __AVX2__
#include <immintrin.h>
#endif

#include "tf2c.h"

#ifdef __GNUC__
#define NORETURN __attribute__((noreturn))
#else
#define NORETURN
#endif

NORETURN
static void error(const char* fmt, ...) {
  va_list ap;
  char buf[4096];
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  va_end(ap);
  fprintf(stderr, "%s\n", buf);
  exit(1);
}

uint tf2c_type_size(Type type) {
  assert(type == INT || type == FLOAT);
  return 4;
}

static void check_shape_eq(const Shape& a, const Shape& b) {
  assert(a.size == b.size);
  assert(a.num_dims == b.num_dims);
  for (uint i = 0; i < a.num_dims; i++)
    assert(a.dims[i] == b.dims[i]);
}

static void check_tensor_type_eq(const Tensor& a, const Tensor& b) {
  assert(a.type == b.type);
  check_shape_eq(a.shape, b.shape);
}

Shape tf2c_shape(const int* dims) {
  Shape shape = tf2c_shape0();
  for (uint i = 0; dims[i] >= 0; i++) {
    if (i >= MAX_DIMS)
      error("more than %d dims", MAX_DIMS);
    shape.dims[i] = dims[i];
    shape.num_dims++;
    shape.size *= dims[i];
  }
  return shape;
}

Shape tf2c_shape0() {
  Shape shape;
  shape.size = 1;
  shape.num_dims = 0;
  return shape;
}

Shape tf2c_shape1(int d0) {
  Shape shape;
  shape.size = d0;
  shape.num_dims = 1;
  shape.dims[0] = d0;
  return shape;
}

Shape tf2c_shape2(int d0, int d1) {
  Shape shape;
  shape.size = d0 * d1;
  shape.num_dims = 2;
  shape.dims[0] = d0;
  shape.dims[1] = d1;
  return shape;
}

Tensor* tf2c_tensor(Type type, Shape shape) {
  Tensor* tensor = (Tensor*)malloc(sizeof(Tensor));
  tensor->type = type;
  tensor->shape = shape;
  uint size = tensor->shape.size * tf2c_type_size(type);
  tensor->alloc = malloc(size + 63);
  tensor->buf = (void*)(((uintptr_t)tensor->alloc + 63) & ~63);
  return tensor;
}

void dump_shape(const Shape& shape) {
  printf("(");
  for (uint i = 0; i < shape.num_dims; i++) {
    if (i)
      printf(", ");
    printf("%d", shape.dims[i]);
  }
  printf(")\n");
}

void dump_tensor(const Tensor& tensor) {
  dump_shape(tensor.shape);
  printf("[");
  for (uint i = 0; i < tensor.shape.size; i++) {
    if (i)
      printf(", ");
    switch (tensor.type) {
    case INT:
      printf("%d", tensor.vec<int>(i));
      break;

    case FLOAT:
      printf("%f", tensor.vec<float>(i));
      break;
    }
  }
  printf("]\n");
}

#define INSTANTIATE(ret, name, args)            \
  template ret name<int> args;                  \
  template ret name<float> args;

#define INSTANTIATE1(ret, name, args)                           \
  INSTANTIATE(ret, name, args);                                 \
  template <>                                                   \
  Tensor* name<void>(const Tensor* a) {                         \
    if (a->type == INT)                                         \
      return name<int>(a);                                      \
    else if (a->type == FLOAT)                                  \
      return name<float>(a);                                    \
    else                                                        \
      error("Unknown type: %d", a->type);                       \
  }

#define INSTANTIATE2(ret, name, args)                           \
  INSTANTIATE(ret, name, args);                                 \
  template <>                                                   \
  Tensor* name<void>(const Tensor* a, const Tensor* b) {        \
    assert(a->type == b->type);                                 \
    if (a->type == INT)                                         \
      return name<int>(a, b);                                   \
    else if (a->type == FLOAT)                                  \
      return name<float>(a, b);                                 \
    else                                                        \
      error("Unknown type: %d", a->type);                       \
  }


template <class T>
void tf2c_fill(Tensor* tensor, T v) {
  for (uint i = 0; i < tensor->shape.size; i++)
    tensor->vec<T>(i) = v;
}

void tf2c_load(Tensor* tensor, const char* fname) {
  FILE* fp = fopen(fname, "rb");
  fread(tensor->buf, tf2c_type_size(tensor->type), tensor->shape.size, fp);
}

template void tf2c_fill<int>(Tensor*, int);
template void tf2c_fill<float>(Tensor*, float);

template <class T>
void tf2c_assign(Tensor* tensor, const T* v) {
  for (uint i = 0; i < tensor->shape.size; i++)
    tensor->vec<T>(i) = v[i];
}

template void tf2c_assign<int>(Tensor*, const int*);
template void tf2c_assign<float>(Tensor*, const float*);

template <class T>
Tensor* tf2c_tanh(const Tensor* a) {
  Tensor* r = tf2c_tensor(a->type, a->shape);
  for (uint i = 0; i < a->shape.size; i++) {
    r->vec<T>(i) = tanh(a->vec<T>(i));
  }
  return r;
}

INSTANTIATE1(Tensor*, tf2c_tanh, (const Tensor* a));

template <class T>
Tensor* tf2c_sigmoid(const Tensor* a) {
  Tensor* r = tf2c_tensor(a->type, a->shape);
  for (uint i = 0; i < a->shape.size; i++) {
    r->vec<T>(i) = 1.0 / (1.0 + exp(-a->vec<T>(i)));
  }
  return r;
}

INSTANTIATE1(Tensor*, tf2c_sigmoid, (const Tensor* a));

template <class T>
Tensor* tf2c_add(const Tensor* a, const Tensor* b) {
  check_tensor_type_eq(*a, *b);
  Tensor* r = tf2c_tensor(a->type, a->shape);
  for (uint i = 0; i < a->shape.size; i++) {
    r->vec<T>(i) = a->vec<T>(i) + b->vec<T>(i);
  }
  return r;
}

INSTANTIATE2(Tensor*, tf2c_add, (const Tensor* a, const Tensor* b));

template <class T>
Tensor* tf2c_mul(const Tensor* a, const Tensor* b) {
  check_tensor_type_eq(*a, *b);
  Tensor* r = tf2c_tensor(a->type, a->shape);
  for (uint i = 0; i < a->shape.size; i++) {
    r->vec<T>(i) = a->vec<T>(i) * b->vec<T>(i);
  }
  return r;
}

INSTANTIATE2(Tensor*, tf2c_mul, (const Tensor* a, const Tensor* b));

#ifdef __AVX2__

static bool tf2c_matmul_avx2(const Tensor* a, const Tensor* b, Tensor* r) {
  uint in = a->shape.dims[0];
  uint jn = a->shape.dims[1];
  uint kn = b->shape.dims[1];
  static const uint IS = 4;
  static const uint KS = 2;
  if (in % IS != 0 || kn % (KS * 8) != 0)
    return false;
  // broadcast: I*J*K = 128M
  // fma: I*J*K/8 = 128M
  // load: I*J*K = 128M
  for (uint i = 0; i < in; i += IS) {
    for (uint k = 0; k < kn; k += KS * 8) {
      __m256 rv[IS][KS] __attribute__((aligned(32))) = { 0 };
      for (uint j = 0; j < jn; j++) {
        for (uint i2 = 0; i2 < IS; i2++) {
          for (uint k2 = 0; k2 < KS; k2++) {
            rv[i2][k2] = _mm256_fmadd_ps(
                _mm256_broadcast_ss(&a->mat<float>(i + i2, j)),
                _mm256_loadu_ps(&b->mat<float>(j, k + k2 * 8)),
                rv[i2][k2]);
          }
        }
      }

      for (uint i2 = 0; i2 < IS; i2++) {
        for (uint k2 = 0; k2 < KS; k2++) {
          _mm256_storeu_ps(
              &r->mat<float>(i + i2, k + k2 * 8),
              _mm256_add_ps(
                  _mm256_loadu_ps(&r->mat<float>(i + i2, k + k2 * 8)),
                  rv[i2][k2]));
        }
      }
    }
  }
  return true;
}

static bool tf2c_vecmatmul_avx2(const Tensor* a, const Tensor* b, Tensor* r) {
  uint in = a->shape.dims[0];
  if (in != 1)
    return false;
  uint jn = a->shape.dims[1];
  uint kn = b->shape.dims[1];
  static const uint KS = 4;
  if (kn % (KS * 8) != 0)
    return false;
#if 0
  // Not good for cache.
  for (uint k = 0; k < kn; k += 8) {
    __m256 rv = {0};
    for (uint j = 0; j < jn; j++) {
      __m256 av = _mm256_broadcast_ss(&a->mat<float>(0, j));
      rv = _mm256_fmadd_ps(av,
                           _mm256_loadu_ps(&b->mat<float>(j, k)),
                           rv);
    }
    _mm256_storeu_ps(&r->mat<float>(0, k), rv);
  }

#elif 1
  // broadcast: J = 3k
  // fma: J*K/8 = 1.5M
  // store: J*K/8 = 1.5M
  // load: J*K/4 = 3M
  for (uint j = 0; j < jn; j++) {
    __m256 av = _mm256_broadcast_ss(&a->mat<float>(0, j));
    for (uint k = 0; k < kn; k += 8) {
      _mm256_storeu_ps(
          &r->mat<float>(0, k),
          _mm256_fmadd_ps(
              av,
              _mm256_loadu_ps(&b->mat<float>(j, k)),
              _mm256_loadu_ps(&r->mat<float>(0, k))));
    }
  }

#else
  // broadcast: J = 3k
  // fma: J*K/8 = 1.5M
  // store: J*KS = 12k
  // load: J*K/8 + J*KS = 1.5M
  for (uint k = 0; k < kn; k += KS * 8) {
    __m256 rv[KS] __attribute__((aligned(32))) = { 0 };
    for (uint j = 0; j < jn; j++) {
      __m256 av = _mm256_broadcast_ss(&a->mat<float>(0, j));
      for (uint k2 = 0; k2 < KS; k2++) {
        rv[k2] = _mm256_fmadd_ps(
            av,
            _mm256_loadu_ps(&b->mat<float>(j, k + k2 * 8)),
            rv[k2]);
      }
    }

    for (uint k2 = 0; k2 < KS; k2++) {
      _mm256_storeu_ps(
          &r->mat<float>(0, k + k2 * 8),
          _mm256_add_ps(
              _mm256_loadu_ps(&r->mat<float>(0, k + k2 * 8)),
              rv[k2]));
    }
  }
#endif
  return true;
}

Tensor* tf2c_vecmatmul_trans_avx2(const Tensor* a, const Tensor* b,
                                  int transpose_a, int transpose_b) {
  if (a->type != FLOAT || transpose_a || !transpose_b)
    return nullptr;
  uint in = a->shape.dims[0];
  if (in != 1)
    return nullptr;
  assert(a->shape.dims[1] == b->shape.dims[1]);
  uint jn = b->shape.dims[0];
  uint kn = b->shape.dims[1];
  if (kn % 8 != 0)
    return nullptr;

  Tensor* r = tf2c_tensor(a->type, tf2c_shape2(1, jn));
  tf2c_fill<float>(r, 0.0);
  for (uint j = 0; j < jn; j++) {
    __m256 rv = { 0 };
    for (uint k = 0; k < kn; k += 8) {
      __m256 av = _mm256_loadu_ps(&a->vec<float>(k));
      __m256 bv = _mm256_loadu_ps(&b->mat<float>(j, k));
      rv = _mm256_fmadd_ps(av, bv, rv);
    }
    float t[8] __attribute__((aligned(32)));
    _mm256_storeu_ps(t, rv);
    float rvt = 0;
    for (uint i = 0; i < 8; i++)
      rvt += t[i];
    r->vec<float>(j) = rvt;
  }
  return r;
}

#endif

template <class T>
Tensor* tf2c_transpose_mat(const Tensor* a) {
  assert(a->shape.num_dims == 2);
  uint in = a->shape.dims[0];
  uint jn = a->shape.dims[1];
  Tensor* tensor = tf2c_tensor(a->type, tf2c_shape2(jn, in));
  for (uint i = 0; i < in; i++) {
    for (uint j = 0; j < jn; j++) {
      tensor->mat<T>(j, i) = a->mat<T>(i, j);
    }
  }
  return tensor;
}

INSTANTIATE1(Tensor*, tf2c_transpose_mat, (const Tensor* a));

template <class T>
Tensor* tf2c_matmul(const Tensor* a, const Tensor* b,
                    int transpose_a, int transpose_b) {
  if (a->shape.num_dims == 1) {
    assert(b->shape.num_dims == 2);
    assert(a->shape.dims[0] == b->shape.dims[0]);
    assert(!transpose_a);
    assert(!transpose_b);
    uint in = a->shape.dims[0];
    uint kn = b->shape.dims[1];
    Tensor* tensor = tf2c_tensor(a->type, tf2c_shape1(kn));
    for (uint k = 0; k < kn; k++) {
      float s = 0;
      for (uint i = 0; i < in; i++) {
        s += a->vec<T>(i) * b->mat<T>(i, k);
      }
      tensor->vec<T>(k) = s;
    }
    return tensor;
  } else {
    Tensor* tensor = nullptr;
#ifdef __AVX2__
    tensor = tf2c_vecmatmul_trans_avx2(a, b, transpose_a, transpose_b);
    if (tensor)
      return tensor;
#endif

    if (transpose_a)
      a = tf2c_transpose_mat<void>(a);
    if (transpose_b)
      b = tf2c_transpose_mat<void>(b);
    assert(a->shape.num_dims == 2);
    assert(b->shape.num_dims == 2);
    assert(a->shape.dims[a->shape.num_dims - 1] == b->shape.dims[0]);
    uint in = a->shape.dims[0];
    uint jn = a->shape.dims[1];
    uint kn = b->shape.dims[1];
    tensor = tf2c_tensor(a->type, tf2c_shape2(in, kn));
    tf2c_fill<float>(tensor, 0.0);

#ifdef __AVX2__
    if (a->type == FLOAT) {
      if (tf2c_vecmatmul_avx2(a, b, tensor))
        return tensor;
      if (tf2c_matmul_avx2(a, b, tensor))
        return tensor;
    }
#endif

    for (uint i = 0; i < in; i++) {
      for (uint j = 0; j < jn; j++) {
        for (uint k = 0; k < kn; k++) {
          tensor->mat<T>(i, k) += a->mat<T>(i, j) * b->mat<T>(j, k);
        }
      }
    }
    return tensor;
  }
}

template<>
Tensor* tf2c_matmul<void>(const Tensor* a, const Tensor* b,
                          int transpose_a, int transpose_b) {
  assert(a->type == b->type);
  if (a->type == INT)
    return tf2c_matmul<int>(a, b, transpose_a, transpose_b);
  else if (a->type == FLOAT)
    return tf2c_matmul<float>(a, b, transpose_a, transpose_b);
  else
    error("Unknown type: %d", a->type);
}
