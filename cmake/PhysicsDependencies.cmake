# Build configuration only; tools/prepare-physics.py owns SDK preparation.
if(ANDROID)
    set(rhythm_physics_triplet arm64-android)
elseif(WIN32)
    set(rhythm_physics_triplet x64-windows)
else()
    message(FATAL_ERROR "Set up and validate the Box2D overlay for this platform before adoption")
endif()
set(RHYTHM_PHYSICS_SDK "${PROJECT_SOURCE_DIR}/out/vcpkg-physics/${rhythm_physics_triplet}"
    CACHE PATH "Validated Box2D vcpkg overlay SDK")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/verify-physics-sdk.py"
    --sdk "${RHYTHM_PHYSICS_SDK}" COMMAND_ERROR_IS_FATAL ANY)
find_package(box2d 3.1.1 EXACT CONFIG REQUIRED
    PATHS "${RHYTHM_PHYSICS_SDK}/share/box2d"
    NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
