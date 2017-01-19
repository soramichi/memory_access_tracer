#if !defined(_VECTOR_H) // double include guard
#define _VECTOR_H

void* create_vector(void);
void add_to_vector(void* _vec, u64 val);
unsigned long* get_data_from_vector(void* _vec);

#endif
