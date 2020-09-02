cmake_minimum_required(VERSION 3.0)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

project(transceiver VERSION 1.0 LANGUAGES C)

# codec2 Library
set(CODEC2_SRC codec2/src)
set(CODEC2_SRCS
${CODEC2_SRC}/codebook.c
${CODEC2_SRC}/codebookd.c
${CODEC2_SRC}/codebookdt.c
${CODEC2_SRC}/codebookge.c
${CODEC2_SRC}/codebookjvm.c
${CODEC2_SRC}/codebooknewamp1.c
${CODEC2_SRC}/codebooknewamp1_energy.c
${CODEC2_SRC}/codebooknewamp2.c
${CODEC2_SRC}/codebooknewamp2_energy.c 
${CODEC2_SRC}/codebookres.c
${CODEC2_SRC}/codebookvq.c
${CODEC2_SRC}/codec2.c
${CODEC2_SRC}/codec2_fft.c
${CODEC2_SRC}/codec2_fifo.c
${CODEC2_SRC}/filter.c
${CODEC2_SRC}/golay23.c
${CODEC2_SRC}/gp_interleaver.c
${CODEC2_SRC}/H_256_768_22.c
${CODEC2_SRC}/HRA_112_112.c
${CODEC2_SRC}/HRAb_396_504.c 
${CODEC2_SRC}/interp.c
${CODEC2_SRC}/kiss_fft.c
${CODEC2_SRC}/kiss_fftr.c
${CODEC2_SRC}/linreg.c
${CODEC2_SRC}/lpc.c
${CODEC2_SRC}/lpcnet_freq.c
${CODEC2_SRC}/lsp.c
${CODEC2_SRC}/mbest.c
${CODEC2_SRC}/newamp1.c
${CODEC2_SRC}/newamp2.c
${CODEC2_SRC}/nlp.c
${CODEC2_SRC}/pack.c
${CODEC2_SRC}/phase.c
${CODEC2_SRC}/phi0.c
${CODEC2_SRC}/postfilter.c
${CODEC2_SRC}/quantise.c 
${CODEC2_SRC}/sine.c
${CODEC2_SRC}/varicode.c
)

add_library(codec2 STATIC ${CODEC2_SRCS})

# crypt2 Library
set(CRYPT2_SRC crypt2/src)
set(CRYPT2_SRCS
${CRYPT2_SRC}/rand.c
${CRYPT2_SRC}/aes.c
)

include_directories(${PROJECT_SOURCE_DIR}/crypt2/include)
add_library(crypt2 STATIC ${CRYPT2_SRCS})

# fec2 Library
set(FEC2_SRC fec2/src)
set(FEC2_SRCS
${FEC2_SRC}/convolutional.c
${FEC2_SRC}/crc16.c
${FEC2_SRC}/golay.c
${FEC2_SRC}/interleaver.c
${FEC2_SRC}/pmatrix.c
${FEC2_SRC}/viterbi.c
)

include_directories(${PROJECT_SOURCE_DIR}/fec2/include)
add_library(fec2 STATIC ${FEC2_SRCS})

add_executable(output m17_tx_simulator.c)

target_link_libraries(output PUBLIC codec2)
target_link_libraries(output PUBLIC crypt2)
target_link_libraries(output PUBLIC fec2)
target_link_libraries(output PUBLIC m)
