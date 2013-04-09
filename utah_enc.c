/* utahenc.c
 *
 * A utah image encoder. Guided by ffmpeg's bmpenc.c file.
 * Strictly uses RGB8 color formats.
 *
 * Daniel Meakin and Austin Hammer
 * 03/14/2013
 * CS 3505
 */

#include "libavutil/imgutils.h"
#include "utah.h"
#include "avcodec.h"
#include "bytestream.h"
#include "internal.h"

/* Initializing Utah Encoder */
static av_cold int utah_encode_init(AVCodecContext *avctx)
{
  /* setting utah context to the private data stored in AV context */
  UtahContext *uctx = avctx->priv_data;

  /* set up the frame */
  avcodec_get_frame_defaults(&uctx->picture);
  avctx->coded_frame = &uctx->picture;

  /* if the format is set to none, return an error */
  if(avctx->pix_fmt == AV_PIX_FMT_NONE)
    return -1;

  /* we only want 8 bits per pixel */
  avctx->bits_per_coded_sample = 8;

  /* exit without an error */
  return 0;
}

/* 
 * A method to encode the frame
 * 
 */
static int utah_encode_frame(AVCodecContext *avctx, AVPacket *pkt,
  		     const AVFrame *pict, int *got_packet)
{
   /* initialize the utah context to the value in av context private data */
   UtahContext *uctx = avctx->priv_data;

   /* set an avframe pointer to the picture variable in utah context */
   AVFrame * const avf = &uctx->picture;

   /* declaring all the variables we use later */
   int image_size, bytes_per_row, total_bytes, i, header_size, ret;

   /* declaring the palette */
   const uint32_t *pal = NULL;
   uint32_t palette256[256];
   int pad, pal_entries = 0;

   /* retrieving the bit count from av context */
   int bit_count = avctx->bits_per_coded_sample;

   /* declaring variable for pointer, buffer */
   uint8_t *ptr, *buf;

   /* setting the pointer to avframe passed in as a param 
      to an avframe pointer variable we can use/modify */
   *avf = *pict;

   avf->pict_type = AV_PICTURE_TYPE_I;
   avf->key_frame = 1;

   /* Palette is being set up, contains 256 colors,
      method is from imgutils, in libavutils */
   avpriv_set_systematic_pal2(palette256, avctx->pix_fmt);
   pal = palette256;

   /* if pal is set up and there is no number for pal_entries, set it up  */
   if (pal && !pal_entries) 
     pal_entries = 1 << bit_count;

   /* setting up the number of bytes per row, using the width passed in
      from av context */
   bytes_per_row = (int64_t)avctx->width;

   /* setting up a pad for the encoder to use as a delimiter */
   pad = (4 - bytes_per_row) & 3;

   /* total bytes for the image */
   image_size = avctx->height * (bytes_per_row + pad);
    

/* setting up the header. we pass in important information to the decoder through
   header, such as the width and height of the frame */
#define SIZE_UTAHFILEHEADER 8
#define SIZE_UTAHINFOHEADER 18

    /* setting header size (file + info + palette) */
    header_size = SIZE_UTAHFILEHEADER + SIZE_UTAHINFOHEADER + (pal_entries << 2);\

    /* setting total file bytes (header + image) */
    total_bytes = image_size + header_size;  

    /* allocating memory for the packet, if it is negative return with error */
    if ((ret = ff_alloc_packet2(avctx, pkt, total_bytes)) < 0)
        return ret;

    /* pointing the buffer to the beginning of the packet, so we can put the
       header into the file */
    buf = pkt->data;

    bytestream_put_le32(&buf, total_bytes);         // File header: file size
    bytestream_put_le32(&buf, header_size);         // File header: header size
    bytestream_put_le32(&buf, SIZE_UTAHINFOHEADER); // Info header: info header size
    bytestream_put_le32(&buf, avctx->width);        // Info header: frame width
    bytestream_put_le32(&buf, avctx->height);       // Info header: frame height
    bytestream_put_le16(&buf, bit_count);           // Info header: bit count
    bytestream_put_le32(&buf, image_size);          // Info header: image size

    /* for loop to insert the palette into the header */
    for (i = 0; i < pal_entries; i++)
      bytestream_put_le32(&buf, pal[i] & 0xFFFFFF); 

    /* setting the pointer at the bottom of the image */
    ptr = avf->data[0] + (avctx->height - 1) * avf->linesize[0];
    /* setting the buffer to just after the header */
    buf = pkt->data + header_size;

    /* for loop, copies the pixels in the image to the packet, adds in
       pad bytes after a row of pixels has been entered. Then it moves 
       the pointer up a row in the image. */
    for(i = 0; i < avctx->height; i++) {
        memcpy(buf, ptr, bytes_per_row);      
        buf += bytes_per_row;
        memset(buf, 0, pad);
        buf += pad; 
        ptr -= avf->linesize[0];
    }

    /* set the flag */
    pkt->flags |= AV_PKT_FLAG_KEY;

    /* a boolean pointer that indicates we got the packet */
    *got_packet = 1;
    return 0;
}

/* all the definitions for the Utah Encoder Codec */
AVCodec ff_utah_encoder = {
    .name           = "utah",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_UTAH,
    .priv_data_size = sizeof(UtahContext),
    .init           = utah_encode_init,
    .encode2        = utah_encode_frame,
    .pix_fmts       = (const enum AVPixelFormat[]) {
        AV_PIX_FMT_RGB8,
        AV_PIX_FMT_NONE
    },
    .long_name      = NULL_IF_CONFIG_SMALL("Utah image"),
};
