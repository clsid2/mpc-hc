# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pv_amr_nb_common_lib


OPTIMIZE_FOR_PERFORMANCE_OVER_SIZE := true

SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS := add.cpp \
	az_lsp.cpp \
	bitno_tab.cpp \
	bitreorder_tab.cpp \
	c2_9pf_tab.cpp \
	div_s.cpp \
	gains_tbl.cpp \
	gc_pred.cpp \
	get_const_tbls.cpp \
	gmed_n.cpp \
	grid_tbl.cpp \
	gray_tbl.cpp \
	int_lpc.cpp \
	inv_sqrt.cpp \
	inv_sqrt_tbl.cpp \
	l_shr_r.cpp \
	log2.cpp \
	log2_norm.cpp \
	log2_tbl.cpp \
	lsfwt.cpp \
	lsp.cpp \
	lsp_az.cpp \
	lsp_lsf.cpp \
	lsp_lsf_tbl.cpp \
	lsp_tab.cpp \
	mult_r.cpp \
	norm_l.cpp \
	norm_s.cpp \
	overflow_tbl.cpp \
	ph_disp_tab.cpp \
	pow2.cpp \
	pow2_tbl.cpp \
	pred_lt.cpp \
	q_plsf.cpp \
	q_plsf_3.cpp \
	q_plsf_3_tbl.cpp \
	q_plsf_5.cpp \
	q_plsf_5_tbl.cpp \
	qua_gain_tbl.cpp \
	reorder.cpp \
	residu.cpp \
	round.cpp \
	shr.cpp \
	shr_r.cpp \
	sqrt_l.cpp \
	sqrt_l_tbl.cpp \
	sub.cpp \
	syn_filt.cpp \
	weight_a.cpp \
	window_tab.cpp

include $(MK)/library.mk


