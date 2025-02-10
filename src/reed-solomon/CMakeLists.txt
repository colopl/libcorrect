set(SRCFILES polynomial.c reed-solomon.c rs_encode.c rs_decode.c)
add_library(correct-reed-solomon OBJECT ${SRCFILES})
