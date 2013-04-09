/* utahdec.c
 *
 * A utah image decoder. Guided by ffmpeg's bmp.c file.
 * Decodes images stored using RGB8.
 *
 * Daniel Meakin and Austin Hammer
 * 3/14/13
 * CS 3505
 */

#include "utah.h"
#include "avcodec.h"
#include "bytestream.h"
#include "internal.h"

/* Initializes the Utah Decoder */
static av_cold int utah_decode_init(AVCodecContext *avctx)
{
  /* Initializing utah context to private data in av context */
    UtahContext *uctx = avctx->priv_data;

    /* setting up the frame */
    avcodec_get_frame_defaults(&uctx->picture);
    avctx->coded_frame = &uctx->picture;

    return 0;
}

/* Method to decode an image stored as a utah image */
static int utah_decode_frame(AVCodecContext *avctx,
                            void *data, int *got_frame,
                            AVPacket *avpkt)
{
    /* setting up the buffer with the data stored in the packet */
    const uint8_t *buf = avpkt->data;
    int buf_size       = avpkt->size;

    /* setting up the utah context, frame */
    UtahContext *uctx  = avctx->priv_data;
    AVFrame *picture   = data;
    AVFrame *avf       = &uctx->picture;

    /* variables for values we'll get from the header */
    unsigned int file_size, header_size;
    int width, height;
    unsigned int depth;
    unsigned int info_header_size;

    /* variable for number of colors in the palette */
    int colors;

    /* variables to assist with navigating the buffer, decoding the image */
    int i, bytes_per_row, linesize, ret;
    uint8_t *ptr;

    /* variable for the start of the buffer */
    const uint8_t *buf_head = buf;

    /* error handling code modified from bmp, if the buffer is too small 
       (smaller than the header size) it won't contain all of the information 
       we need and we won't be able to properly decode the image */
    if (buf_size < 8) {
        av_log(avctx, AV_LOG_ERROR, "Buffer size is too small (%d)\n", buf_size);
        return AVERROR_INVALIDDATA;
    }

    /* pulling the size of the file from the header */
    file_size = bytestream_get_le32(&buf);

    /* error handling code from bmp, if the file size is smaller than the
       buffer size chances are we won't have enough data to decode. We will try
       to decode anyway, even though our data may be insufficient. Sets the file
       size to the buffer size to prevent seg faults */
    if (buf_size < file_size) {
        av_log(avctx, AV_LOG_ERROR, "Not enough data (%d < %d), trying to decode anyway\n",
               buf_size, file_size);
        file_size = buf_size;
    }

    /* grabbing header size and info header size from the header */
    header_size  = bytestream_get_le32(&buf);         
    info_header_size = bytestream_get_le32(&buf);  

    /* grabbing the width and height of the image from the header */
    width  = bytestream_get_le32(&buf);               
    height = bytestream_get_le32(&buf);               

    /* grabbing the number of bytes per pixel from the header */
    depth = bytestream_get_le16(&buf);
    
    /* setting the width and height of the context to the values pulled from the buffer */
    avctx->width  = width;
    avctx->height = height;

    /* setting the pixel format to RGB8 */
    avctx->pix_fmt = AV_PIX_FMT_RGB8;    

    if (avf->data[0])
        avctx->release_buffer(avctx, avf);

    avf->reference = 0;
    if ((ret = ff_get_buffer(avctx, avf)) < 0) {
        av_log(avctx, AV_LOG_ERROR, "get_buffer() failed\n");
        return ret;
    }

    avf->pict_type = AV_PICTURE_TYPE_I;
    avf->key_frame = 1;

    /* multiplying width by 8, adding 11111 and dividing by 1000, then and with 00, 
       accounts for the pad */
    bytes_per_row = ((avctx->width * depth + 31) / 8) & ~3;

    /* setting the pointer to the bottom of the frame */
    ptr      = avf->data[0] + (avctx->height - 1) * avf->linesize[0];
    linesize = -avf->linesize[0];

    /* shifts 1 to the left 8 times, for 256 colors */
    colors = 1 << depth;

    memset(avf->data[1], 0, 1024);
    
    /* moving the buffer to where the palette is located */
    buf = buf_head + 8 + info_header_size;
        
    /* iterating through the buffer, pulling each color out, and storing
       them in the frame. */
    for (i = 0; i < colors; i++)
        ((uint32_t*)avf->data[1])[i] = 0xFFU << 24 | bytestream_get_le32(&buf);
        
    /* moving the buffer to just past the header */
    buf = buf_head + header_size;
        
    /* copying the rows back from the buffer to the packet. When a row is finished,
       moves on to the next row */
    for (i = 0; i < avctx->height; i++) {
      memcpy(ptr, buf, bytes_per_row);
      buf += bytes_per_row;
      ptr += linesize;
    }
    
    /* putting the data in utah context into the frame */
    *picture = uctx->picture;

    /* a boolean acknowledging we got the frame */
    *got_frame = 1;

    /* returns without error as long as buf_size is positive */
    return buf_size;
}

/* Method to end decoding */
static av_cold int utah_decode_end(AVCodecContext *avctx)
{
    UtahContext* uctx = avctx->priv_data;

    /* releases buffer if utah context contains data */
    if (uctx->picture.data[0])
        avctx->release_buffer(avctx, &uctx->picture);

    return 0;
}

/* Contains codec definitions for the utah decoder */
AVCodec ff_utah_decoder = {
    .name           = "utah",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_UTAH,
    .priv_data_size = sizeof(UtahContext),
    .init           = utah_decode_init,
    .close          = utah_decode_end,
    .decode         = utah_decode_frame,
    .capabilities   = CODEC_CAP_DR1 /*| CODEC_CAP_DRAW_HORIZ_BAND*/,
    .long_name      = NULL_IF_CONFIG_SMALL("Utah image"),
};
