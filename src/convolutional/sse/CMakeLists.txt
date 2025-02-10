set(SRCFILES lookup.c convolutional.c cv_encode.c cv_decode.c)
add_library(correct-convolutional-sse OBJECT ${SRCFILES})
