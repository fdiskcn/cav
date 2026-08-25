/****************************************************************************
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

// No-CUDA / no-GPU check for the ovrtx engine. Passes if initialize() either
// succeeds (RTX GPU present) or fails with an error string (no libcuda).
// Must not crash or throw.

#include "ovrtx_engine.h"

#include <iostream>

int main()
{
    Mayo::Ovrtx::OvrtxEngine engine;
    const bool ready = engine.initialize();
    if (ready) {
        std::cout << "PASS  ovrtx initialize succeeded (NVIDIA GPU present)\n";
        engine.shutdown();
        return 0;
    }

    const std::string& err = engine.lastError();
    if (err.empty()) {
        std::cout << "FAIL  ovrtx initialize failed without an error message\n";
        return 1;
    }

    std::cout << "PASS  ovrtx initialize failed without CUDA/GPU: " << err << '\n';
    return 0;
}
