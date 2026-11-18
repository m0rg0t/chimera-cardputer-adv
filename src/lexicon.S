/* Keep the packed dictionary in flash/rodata on ESP32-S3. */
#ifdef __APPLE__
.section __TEXT,__const
.globl _chimera_lexicon
_chimera_lexicon:
#else
.section .rodata.chimera_lexicon,"a"
.global chimera_lexicon
chimera_lexicon:
#endif
.incbin "src/lexicon.generated.bin"
