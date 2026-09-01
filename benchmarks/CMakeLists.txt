# Copyright 2026 Summon Software Labs
# Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
add_executable(ah-bench bench.cpp)
target_link_libraries(ah-bench PRIVATE AcceleratorHealth::AcceleratorHealth)
ah_cxx_flags(ah-bench)
