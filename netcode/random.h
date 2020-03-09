#pragma once

#include <memory>
#include <random>

class Random
{
private:
    static std::unique_ptr<Random> instance;
    static Random& getInstance();

    std::default_random_engine engine;

public:
    Random();

    static std::default_random_engine::result_type get();
};
