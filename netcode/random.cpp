#include "random.h"

std::unique_ptr<Random> Random::instance;

Random& Random::getInstance()
{
    if (!Random::instance) {
        Random::instance = std::make_unique<Random>();
    }
    return *Random::instance;
}

Random::Random()
    : engine(std::random_device()())
{}

std::default_random_engine::result_type Random::get()
{
    return Random::getInstance().engine();
}
