#ifndef LIBDRAGON_AUDIO_VADPCM_INTERNAL_H
#define LIBDRAGON_AUDIO_VADPCM_INTERNAL_H

/** @brief A vector of audio samples */
typedef struct __attribute__((aligned(8))) {
	int16_t v[8];						///< Samples
} wav64_vadpcm_vector_t;

/** @brief Extended header for a WAV64 file with VADPCM compression. */
typedef struct __attribute__((packed, aligned(8))) {
	int8_t npredictors;					///< Number of predictors
	int8_t order;						///< Order of the predictors
	uint16_t padding;					///< padding
	uint32_t padding1;					///< padding1
	wav64_vadpcm_vector_t loop_state[2];///< State at the loop point
	wav64_vadpcm_vector_t codebook[];	///< Codebook of the predictors
} wav64_header_vadpcm_t;

/** @brief WAV64 VADPCM decoding state (for a single mixer channel) */
typedef struct {
	wav64_vadpcm_vector_t state[2];		///< Current decompression state
	int bitpos;							///< Current bit position in the input buffer
} wav64_state_vadpcm_t;


void wav64_vadpcm_init(wav64_t *wav, int state_size);
void wav64_vadpcm_close(wav64_t *wav);
int wav64_vadpcm_get_bitrate(wav64_t *wav);

#endif
