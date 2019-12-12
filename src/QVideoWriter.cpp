#include "QVideoWriter.h"

#include <iostream>

QVideoWriter::QVideoWriter(): endcode{ 0, 0, 1, 0xb7 } {
	codec = nullptr;
	c = nullptr;
	video_file = nullptr;
	frame = nullptr;
	pkt = nullptr;
}

void QVideoWriter::encode_frame(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt, FILE *outfile) {
	int ret;

	/* send the frame to the encoder */
	if (frame)
		printf("Send frame %3" PRId64 "\n", frame->pts);

	ret = avcodec_send_frame(enc_ctx, frame);
	if (ret < 0) {
		throw VideoWriterError("Error sending a frame for encoding\n");
	}

	while (ret >= 0) {
		ret = avcodec_receive_packet(enc_ctx, pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return;
		else if (ret < 0) {
			throw VideoWriterError("Error during encoding\n");
		}

		printf("Write packet %3" PRId64 " (size=%5d)\n", pkt->pts, pkt->size);
		fwrite(pkt->data, 1, pkt->size, outfile);
		av_packet_unref(pkt);
	}
}

void QVideoWriter::createVideo(QString filename, int width, int height, int fps) {
	video_width = width;
	video_height = height;
	video_fps = fps;

	video_filename = filename;

	int ret;
	//Setup FFmpeg correctly
	QString codec_name = "libx264";
	codec = avcodec_find_encoder_by_name(codec_name.toStdString().c_str());
	if (!codec) {
		QString error_msg = QString("Codec %1 not found\n").arg(codec_name);
		throw VideoWriterError( error_msg.toStdString() );
	}

	c = avcodec_alloc_context3(codec);
	if (!c) {
		throw VideoWriterError( "Could not allocate video codec context\n");
	}

	pkt = av_packet_alloc();
	if (!pkt)
		throw VideoWriterError( "Could not allocate packet buffer\n");

	/* put sample parameters */
	c->bit_rate = 400000;
	/* resolution must be a multiple of two */
	c->width = width;
	c->height = height;
	/* frames per second */
	c->time_base = (AVRational){1, fps};
	c->framerate = (AVRational){fps, 1};

	/* emit one intra frame every ten frames
	* check frame pict_type before passing frame
	* to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
	* then gop_size is ignored and the output of encoder
	* will always be I frame irrespective to gop_size
	*/
	c->gop_size = 10;
	c->max_b_frames = 1;
	c->pix_fmt = AV_PIX_FMT_ARGB;

	if (codec->id == AV_CODEC_ID_H264)
		av_opt_set(c->priv_data, "preset", "slow", 0);

	/* open it */
	ret = avcodec_open2(c, codec, NULL);
	if (ret < 0) {
		QString error_msg = QString("Could not open codec erro code %1\n").arg(ret);
		throw VideoWriterError( error_msg.toStdString() );
	}

	video_file = fopen(video_filename.toStdString().c_str(), "wb");
	if (!video_file) {
		QString error_msg = QString("Could not open file %1\n").arg(video_filename);
		throw VideoWriterError( error_msg.toStdString() );
	}

	frame = av_frame_alloc();
	if (!frame) {
		throw VideoWriterError( "Could not allocate video frame\n");
	}
	frame->format = c->pix_fmt;
	frame->width  = c->width;
	frame->height = c->height;

	ret = av_frame_get_buffer(frame, 32);
	if (ret < 0) {
		throw VideoWriterError( "Could not allocate the video frame data\n");
	}

	frame_cnt = 0;
}

void QVideoWriter::addFrame(const QImage &frame_image) {
	if(frame_image.width()!=video_width || frame_image.height()!=video_height)
	{
		throw VideoWriterError("Wrong image size!\n");
	}

	int ret;
	ret = av_frame_make_writable(frame);
	if (ret < 0)
		throw VideoWriterError("Frame is now writeable!\n");

	//Todo put image data in frame
	memcpy(frame->data, frame_image.bits(), sizeof(frame->data));
	frame->pts = frame_cnt;

	/* encode the image */
	encode_frame(c, frame, pkt, video_file);
	frame_cnt += 1;
}

void QVideoWriter::finish() {
	encode_frame(c, NULL, pkt, video_file);

	/* add sequence end code to have a real MPEG file */
	if (codec->id == AV_CODEC_ID_MPEG1VIDEO || codec->id == AV_CODEC_ID_MPEG2VIDEO)
		fwrite(endcode, 1, sizeof(&endcode), video_file);
	fclose(video_file);

	avcodec_free_context(&c);
	av_frame_free(&frame);
	av_packet_free(&pkt);
}

