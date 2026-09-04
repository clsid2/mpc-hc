# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvamrwbdecoder


OPTIMIZE_FOR_PERFORMANCE_OVER_SIZE := true

SRCDIR := ../../src
INCSRCDIR := ../../include
SRCS := agc2_amr_wb.cpp \
	band_pass_6k_7k.cpp \
	dec_acelp_2p_in_64.cpp \
	dec_acelp_4p_in_64.cpp \
	dec_alg_codebook.cpp \
	dec_gain2_amr_wb.cpp \
	decoder_amr_wb.cpp \
	deemphasis_32.cpp \
	dtx_decoder_amr_wb.cpp \
	get_amr_wb_bits.cpp \
	highpass_400hz_at_12k8.cpp \
	highpass_50hz_at_12k8.cpp \
	homing_amr_wb_dec.cpp \
	interpolate_isp.cpp \
	isf_extrapolation.cpp \
	isp_az.cpp \
	isp_isf.cpp \
	lagconceal.cpp \
	low_pass_filt_7k.cpp \
	median5.cpp \
	mime_io.cpp \
	noise_gen_amrwb.cpp \
	normalize_amr_wb.cpp \
	oversamp_12k8_to_16k.cpp \
	phase_dispersion.cpp \
	pit_shrp.cpp \
	pred_lt4.cpp \
	preemph_amrwb_dec.cpp \
	pvamrwb_math_op.cpp \
	pvamrwbdecoder.cpp \
	q_gain2_tab.cpp \
	qisf_ns.cpp \
	qisf_ns_tab.cpp \
	qpisf_2s.cpp \
	qpisf_2s_tab.cpp \
	scale_signal.cpp \
	synthesis_amr_wb.cpp \
	voice_factor.cpp \
	wb_syn_filt.cpp \
	weight_amrwb_lpc.cpp


HDRS := decoder_amr_wb.h pvamrwbdecoder_api.h

include $(MK)/library.mk

