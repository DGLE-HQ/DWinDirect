// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX



// TODO: reference additional headers your program requires here
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <climits>
#include <cstring>
#include <cmath>
#include <utility>
#include <type_traits>
#include <limits>
#include <memory>
#include <iterator>
#include <vector>
#include <queue>
#include <deque>
#include <list>
#include <algorithm>
#include <numeric>
#include <functional>
#include <optional>
#include <variant>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <wrl/client.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include "d3dx12.h"
#include "HRESULT.h"
#define DISABLE_MATRIX_SWIZZLES
#if !__INTELLISENSE__ 
#include "vector math.h"
#endif