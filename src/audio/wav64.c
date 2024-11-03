/**
 * @file wav64.c
 * @brief Support for WAV64 audio files
 * @ingroup mixer
 */

#include "wav64.h"
#include "wav64internal.h"
#include "wav64_opus_internal.h"
#include "mixer.h"
#include "mixer_internal.h"
#include "dragonfs.h"
#include "n64sys.h"
#include "dma.h"
#include "samplebuffer.h"
#include "debug.h"
#include "utils.h"
#include "rspq.h"
#include "asset_internal.h"
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdalign.h>
#include <fcntl.h>
#include <unistd.h>

/** @brief Set to 1 to use the reference C decode for VADPCM */
#define VADPCM_REFERENCE_DECODER     0

/** ID of a standard WAV file */
#define WAV_RIFF_ID   "RIFF"
/** ID of a WAVX file (big-endian WAV) */
#define WAV_RIFX_ID   "RIFX"

/** @brief Profile of DMA usage by WAV64, used for debugging purposes. */
int64_t __wav64_profile_dma = 0;

#if VADPCM_REFERENCE_DECODER
/** @brief VADPCM decoding errors */
typedef enum {
    // No error (success). Equal to 0.
    kVADPCMErrNone,

    // Invalid data.
    kVADPCMErrInvalidData,

    // Predictor order is too large.
    kVADPCMErrLargeOrder,

    // Predictor count is too large.
    kVADPCMErrLargePredictorCount,

    // Data uses an unsupported / unknown version of VADPCM.
    kVADPCMErrUnknownVersion,

    // Invalid encoding parameters.
    kVADPCMErrInvalidParams,
} vadpcm_error;

// Extend the sign bit of a 4-bit integer.
static int vadpcm_ext4(int x) {
    return x > 7 ? x - 16 : x;
}

// Clamp an integer to a 16-bit range.
static int vadpcm_clamp16(int x) {
    if (x < -0x8000 || 0x7fff < x) {
        return (x >> (sizeof(int) * CHAR_BIT - 1)) ^ 0x7fff;
    }
    return x;
}

__attribute__((unused))
static vadpcm_error vadpcm_decode(int predictor_count, int order,
                           const wav64_vadpcm_vector_t *restrict codebook,
                           wav64_vadpcm_vector_t *restrict state,
                           size_t frame_count, int16_t *restrict dest,
                           const void *restrict src) {
    const uint8_t *sptr = src;
    for (size_t frame = 0; frame < frame_count; frame++) {
        const uint8_t *fin = sptr + 9 * frame;

        // Control byte: scaling & predictor index.
        int control = fin[0];
        int scaling = control >> 4;
        int predictor_index = control & 15;
        if (predictor_index >= predictor_count) {
            return kVADPCMErrInvalidData;
        }
        const wav64_vadpcm_vector_t *predictor =
            codebook + order * predictor_index;

        // Decode each of the two vectors within the frame.
        for (int vector = 0; vector < 2; vector++) {
            int32_t accumulator[8];
            for (int i = 0; i < 8; i++) {
                accumulator[i] = 0;
            }

            // Accumulate the part of the predictor from the previous block.
            for (int k = 0; k < order; k++) {
                int sample = state->v[8 - order + k];
                for (int i = 0; i < 8; i++) {
                    accumulator[i] += sample * predictor[k].v[i];
                }
            }

            // Decode the ADPCM residual.
            int residuals[8];
            for (int i = 0; i < 4; i++) {
                int byte = fin[1 + 4 * vector + i];
                residuals[2 * i] = vadpcm_ext4(byte >> 4);
                residuals[2 * i + 1] = vadpcm_ext4(byte & 15);
            }

            // Accumulate the residual and predicted values.
            const wav64_vadpcm_vector_t *v = &predictor[order - 1];
            for (int k = 0; k < 8; k++) {
                int residual = residuals[k] << scaling;
                accumulator[k] += residual << 11;
                for (int i = 0; i < 7 - k; i++) {
                    accumulator[k + 1 + i] += residual * v->v[i];
                }
            }

            // Discard fractional part and clamp to 16-bit range.
            for (int i = 0; i < 8; i++) {
                int sample = vadpcm_clamp16(accumulator[i] >> 11);
                dest[16 * frame + 8 * vector + i] = sample;
                state->v[i] = sample;
            }
        }
    }
    return 0;
}
#else

static inline void rsp_vadpcm_decompress(void *input, int16_t *output, bool stereo, int nframes, 
	wav64_vadpcm_vector_t *state, wav64_vadpcm_vector_t *codebook)
{
	assert(nframes > 0 && nframes <= 256);
	rspq_write(__mixer_overlay_id, 0x1,
		PhysicalAddr(input), 
		PhysicalAddr(output) | (nframes-1) << 24,
		PhysicalAddr(state)  | (stereo ? 1 : 0) << 31,
		PhysicalAddr(codebook));
}

#endif /* VADPCM_REFERENCE_DECODER */

void raw_waveform_read(samplebuffer_t *sbuf, int current_fd, int wpos, int wlen, int bps) {
	uint8_t* ram_addr = (uint8_t*)samplebuffer_append(sbuf, wlen);
	int bytes = wlen << bps;

	// FIXME: remove CachedAddr() when read() supports uncached addresses
	uint32_t t0 = TICKS_READ();
	read(current_fd, CachedAddr(ram_addr), bytes);
	__wav64_profile_dma += TICKS_READ() - t0;
}

void raw_waveform_read_address(samplebuffer_t *sbuf, int base_rom_addr, int wpos, int wlen, int bps) {
	uint32_t rom_addr = base_rom_addr + (wpos << bps);
	uint8_t* ram_addr = (uint8_t*)samplebuffer_append(sbuf, wlen);
	int bytes = wlen << bps;

	uint32_t t0 = TICKS_READ();
	// Run the DMA transfer. We rely on libdragon's PI DMA function which works
	// also for misaligned addresses and odd lengths.
	// The mixer/samplebuffer guarantees that ROM/RAM addresses are always
	// on the same 2-byte phase, as the only requirement of dma_read.
	dma_read(ram_addr, rom_addr, bytes);
	__wav64_profile_dma += TICKS_READ() - t0;
}

static void waveform_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
	wav64_t *wav = (wav64_t*)ctx;
	int bps = (wav->wave.bits == 8 ? 0 : 1) + (wav->wave.channels == 2 ? 1 : 0);
	if (seeking) {
		lseek(wav->current_fd, wav->base_offset + (wpos << bps), SEEK_SET);
	}
	raw_waveform_read(sbuf, wav->current_fd, wpos, wlen, bps);
}

static void waveform_vadpcm_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
	wav64_t *wav = (wav64_t*)ctx;
	wav64_header_vadpcm_t *vhead = (wav64_header_vadpcm_t*)wav->ext;

	if (seeking) {
		if (wpos == 0) {
			memset(&vhead->state, 0, sizeof(vhead->state));
			lseek(wav->current_fd, wav->base_offset, SEEK_SET);
		} else {
			assertf(wpos == wav->wave.len - wav->wave.loop_len,
				"wav64: seeking to %x not supported (%x %x)\n", wpos, wav->wave.len, wav->wave.loop_len);
			memcpy(&vhead->state, &vhead->loop_state, sizeof(vhead->state));
			lseek(wav->current_fd, (wav->wave.len - wav->wave.loop_len) / 16 * 9, SEEK_CUR);
		}
	}

	// Round up wlen to 32 because our RSP decompressor only supports multiples
	// of 32 samples (2 frames) because of DMA alignment issues. audioconv64
	// makes sure files are padded to that length, so this is valid also at the
	// end of the file.
	wlen = ROUND_UP(wlen, 32);
	if (wlen == 0) return;

	// Maximum number of VADPCM frames that can be decompressed in a single
	// RSP call. Keep this in sync with rsp_mixer.S.
	enum { MAX_VADPCM_FRAMES = 94 };

	bool highpri = false;
	while (wlen > 0) {
		// Calculate number of frames to decompress in this iteration
		int max_vadpcm_frames = (wav->wave.channels == 1) ? MAX_VADPCM_FRAMES : MAX_VADPCM_FRAMES / 2;
		int nframes = MIN(wlen / 16, max_vadpcm_frames);

		// Acquire destination buffer from the sample buffer
		int16_t *dest = (int16_t*)samplebuffer_append(sbuf, nframes*16);

		// Calculate source pointer at the end of the destination buffer.
		// VADPCM decoding can be safely made in-place, so no auxillary buffer
		// is necessary.
		int src_bytes = 9 * nframes * wav->wave.channels;
		void *src = (void*)dest + ((nframes*16) << SAMPLES_BPS_SHIFT(sbuf)) - src_bytes;

		// Fetch compressed data
		// FIXME: remove CachedAddr() when read() supports uncached addresses
		int read_bytes = read(wav->current_fd, CachedAddr(src), src_bytes);
		assertf(src_bytes == read_bytes, "invalid read past end: %d vs %d", src_bytes, read_bytes);

		#if VADPCM_REFERENCE_DECODER
		if (wav->wave.channels == 1) {
			vadpcm_error err = vadpcm_decode(
				vhead->npredictors, vhead->order, vhead->codebook, vhead->state,
				nframes, dest, src);
			assertf(err == 0, "VADPCM decoding error: %d\n", err);
		} else {
			assert(wav->wave.channels == 2);
			int16_t uncomp[2][16];
			int16_t *dst = dest;

			for (int i=0; i<nframes; i++) {
				for (int j=0; j<2; j++) {
					vadpcm_error err = vadpcm_decode(
						vhead->npredictors, vhead->order, vhead->codebook + 8*j, &vhead->state[j],
						1, uncomp[j], src);
					assertf(err == 0, "VADPCM decoding error: %d\n", err);
					src += 9;
				}
				for (int j=0; j<16; j++) {
					*dst++ = uncomp[0][j];
					*dst++ = uncomp[1][j];
				}
			}
		}
		#else
		// Switch to highpri as late as possible
		if (!highpri) {
			rspq_highpri_begin();
			highpri = true;
		}
		rsp_vadpcm_decompress(src, dest, wav->wave.channels==2, nframes, vhead->state, vhead->codebook);
		#endif

		wlen -= 16*nframes;
		wpos += 16*nframes;
	}

	if (highpri)
		rspq_highpri_end();

    if (wav->wave.loop_len && wpos >= wav->wave.len) {
        assert(wav->wave.loop_len == wav->wave.len);
        samplebuffer_undo(sbuf, wpos - wav->wave.len);
    }
}

void wav64_open(wav64_t *wav, const char *file_name) {
	memset(wav, 0, sizeof(*wav));

	// Open the input file.
	int file_handle = must_open(file_name);
	wav64_header_t head = {0};
	read(file_handle, &head, sizeof(head));
	if (memcmp(head.id, WAV64_ID, 4) != 0) {
		assertf(memcmp(head.id, WAV_RIFF_ID, 4) != 0 && memcmp(head.id, WAV_RIFX_ID, 4) != 0,
			"wav64 %s: use audioconv64 to convert to wav64 format", file_name);
		assertf(0, "wav64 %s: invalid ID: %02x%02x%02x%02x\n",
			file_name, head.id[0], head.id[1], head.id[2], head.id[3]);
	}
	assertf(head.version == WAV64_FILE_VERSION, "wav64 %s: invalid version: %02x\n",
		file_name, head.version);

	wav->wave.name = file_name;
	wav->wave.channels = head.channels;
	wav->wave.bits = head.nbits;
	wav->wave.frequency = head.freq;
	wav->wave.len = head.len;
	wav->wave.loop_len = head.loop_len; 
	wav->current_fd = file_handle;
	wav->base_offset = head.start_offset;
	wav->format = head.format;

	switch (head.format) {
	case WAV64_FORMAT_RAW:
		wav->wave.read = waveform_read;
		wav->wave.ctx = wav;
		break;

	case WAV64_FORMAT_VADPCM: {
		wav64_header_vadpcm_t vhead = {0};
		read(file_handle, &vhead, sizeof(vhead));
		int codebook_size = vhead.npredictors * vhead.order * head.channels * sizeof(wav64_vadpcm_vector_t);

		void *ext = malloc_uncached(sizeof(vhead) + codebook_size);
		memcpy(ext, &vhead, sizeof(vhead));
		// FIXME: remove CachedAddr() when read() supports uncached addresses
		read(file_handle, CachedAddr(ext + sizeof(vhead)), codebook_size);
		wav->ext = ext;
		wav->wave.read = waveform_vadpcm_read;
		wav->wave.ctx = wav;
		assertf(head.loop_len == 0 || head.loop_len % 16 == 0, 
			"wav64 %s: invalid loop length: %ld\n", file_name, head.loop_len);
	}	break;

	case WAV64_FORMAT_OPUS: {
		wav64_opus_init(wav, file_handle);
	}	break;
	
	default:
		assertf(0, "wav64 %s: invalid format: %02x\n", file_name, head.format);
	}

	lseek(wav->current_fd, wav->base_offset, SEEK_SET);
}

void wav64_play(wav64_t *wav, int ch)
{
	// Update the context pointer, so that we try to catch cases where the
	// wav64_t instance was moved.
	wav->wave.ctx = wav;
	mixer_ch_play(ch, &wav->wave);
}

void wav64_set_loop(wav64_t *wav, bool loop) {
	wav->wave.loop_len = loop ? wav->wave.len : 0;

	// Odd loop lengths are not supported for 8-bit waveforms because they would
	// change the 2-byte phase between ROM and RDRAM addresses during loop unrolling.
	// We shorten the loop by 1 sample which shouldn't matter.
	// Notice that audioconv64 does the same during conversion.
	if (wav->wave.bits == 8 && wav->wave.loop_len & 1)
		wav->wave.loop_len -= 1;
}

int wav64_get_bitrate(wav64_t *wav) {
	if (wav->ext) {
		switch (wav->format) {
		case WAV64_FORMAT_VADPCM:
			return wav->wave.frequency * wav->wave.channels * 72 / 16;
		case WAV64_FORMAT_OPUS:
			return wav64_opus_get_bitrate(wav);
		}
	}
	return wav->wave.frequency * wav->wave.channels * wav->wave.bits;
}

void wav64_close(wav64_t *wav)
{
	if (wav->ext) {
		switch (wav->format) {
		case WAV64_FORMAT_VADPCM:
			free_uncached(wav->ext);
			break;
		case WAV64_FORMAT_OPUS:
			wav64_opus_close(wav);
			break;
		}
		wav->ext = NULL;
		if (wav->current_fd >= 0) {
			close(wav->current_fd);
			wav->current_fd = -1;
		}
	}
}