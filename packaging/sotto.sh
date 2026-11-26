# The sotto vocoder Ragtime's voice messages are coded with: codec 2 is
# sotto v3.8 (docs/PROTOCOL.md, "Voice"), 16 kHz speech with the 4-8 kHz
# hiss band, and the same library plays codec 1 (v3.6's frames) at 8 kHz.
# It also has sotto 1200, 600 and 400 with the hiss band, float, both
# ways (sotto_lr_enc16 / _dec16, sotto_vlr_enc16 / _dec16): codecs 3, 4
# and 5. From 8a8c677 the low rates' pitch fixes (an onset's voiced pitch,
# outliers left out of the 600/400 pitch line), encoder only; from
# aa44cdb a cap on 600/400 hiss in frames with no hiss bits, decoder
# only. From e707134 better encoders only: the pitch tracker all four
# rates share lifts short sub-octave runs and holds word-final creak
# (sotto's "v3.10 encoder", still sending v3.8 frames), the 2400 encoder
# picks the shape index by a peak-weighted error, and 600/400 weigh creak
# down in the pitch line; the decoders and the exported API are
# unchanged. The same frame
# layout throughout, so clips sent with any earlier pin play the same:
# every decoder here gives sound bit-identical to aa44cdb's for the same
# frames, codec 1's included.
# Ragtime needs a library whose sotto_format() says "sotto v3.8, shape
# 9-9-8-8", so the variant is named here rather than left to sotto's
# Makefile. From this commit sotto's generated C tables are committed, so
# the build needs no Python. Ragtime loads libsotto at run time and never
# links it (LGPL through Codec 2); the packages carry it beside ragtime with
# its licences. Sourced by build-sotto.sh.
SOTTO_REPO=${SOTTO_REPO:-https://github.com/baitisj/sotto.git}
SOTTO_COMMIT=e707134fd7f9e2721b55b9d983b77a8aa5593552
SOTTO_VARIANT=v3.8
