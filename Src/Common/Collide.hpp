#pragma once
#include <algorithm>


namespace LV {

template<typename VecType>
bool calcBoxToBoxCollide(const VecType vec1_min, const VecType vec1_max, 
    const VecType vec2_min, const VecType vec2_max, bool axis[VecType::length()] = nullptr
) {

    using ValType = VecType::Type;

    ValType max_delta = 0;
    ValType result = 0;

    for(int iter = 0; iter < VecType::length(); iter++) {
        ValType left = vec2_min[iter]-vec1_max[iter];
        ValType right = vec1_min[iter]-vec2_max[iter];
        result = std::max(result, std::max(left, right));

        if(axis)
            axis[iter] = std::min(left, right) < ValType(0);
    }

    return result <= ValType(0);
}


template<typename VecType>
bool calcBoxToBoxCollideWithDelta(const VecType vec1_min, const VecType vec1_max, 
    const VecType vec2_min, const VecType vec2_max, VecType vec1_speed, 
    typename VecType::value_type *delta, typename VecType::value_type deltaBias, bool axis[VecType::length()]
) {
    using ValType = VecType::value_type;

    ValType max_delta = 0;

    for(int iter = 0; iter < VecType::length(); iter++) {
        ValType left = vec2_min[iter]-vec1_max[iter];
        ValType right = vec1_min[iter]-vec2_max[iter];
        ValType new_detla = (right > left ? -right : left)*deltaBias/vec1_speed[iter];
        max_delta = std::max(max_delta, new_detla);
    }

    ValType result = 0;
    for(int iter = 0; iter < VecType::length(); iter++) {
        ValType left = vec2_min[iter]-(vec1_max[iter]+vec1_speed[iter]*max_delta/deltaBias);
        ValType right = (vec1_min[iter]+vec1_speed[iter]*max_delta/deltaBias)-vec2_max[iter];

        if(axis)
            axis[iter] = std::min(left, right) < ValType(0);

        result = std::min(std::max(left, right), result);
    }

    *delta = max_delta;
    return result < ValType(0);
}
    
}