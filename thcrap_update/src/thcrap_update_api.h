#pragma once

#ifdef THCRAP_UPDATE_EXPORTS
# define THCRAP_UPDATE_API __declspec(dllexport)
#else
# define THCRAP_UPDATE_API __declspec(dllimport)
#endif
