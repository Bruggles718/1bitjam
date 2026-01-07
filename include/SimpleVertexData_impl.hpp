#pragma once

#include "SceneObject.hpp"
#include <iostream>

template<int T>
SimpleVertexData<T>::SimpleVertexData(
    std::vector<float> i_vertex_buffer, 
    int i_stride,
    std::array<int, T> i_offsets
) : VertexData(i_vertex_buffer) {
    m_stride = i_stride;
    m_offsets = i_offsets;
}

template<int T>
SimpleVertexData<T>::~SimpleVertexData() {
    
}

template<int T>
void SimpleVertexData<T>::send_to_gpu() {
    
}

template<int T>
void SimpleVertexData<T>::setup_for_send() {
    // select vertex array object
    
}

template<int T>
void SimpleVertexData<T>::send_buffer() {
    // copy data to gpu
    
}

template<int T>
void SimpleVertexData<T>::send_attributes() {
    
}

template<int T>
int SimpleVertexData<T>::get_attribute_size(int i_idx) {
    return (i_idx + 1 < T ? m_offsets[i_idx + 1] : m_stride) - m_offsets[i_idx];
}