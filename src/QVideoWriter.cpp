#include "QVideoWriter.h"

#include <iostream>

QVideoWriter::QVideoWriter(): endcode{ 0, 0, 1, 0xb7 } {
	codec = nullptr;
	c = nullptr;
	output_c = nullptr;
	output_f = nullptr;
	video_file = nullptr;
	video_stream = nullptr;
	frame = nullptr;
	pkt = nullptr;
	img_convert_ctx = nullptr;

	#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58,9,100)
		avcodec_register_all();
		av_register_all();
	#endif
}

void QVideoWriter::encode_frame(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt, FILE *outfile) {
	int ret;

	/* send the frame to the encoder */
	//if (frame)
	//	printf("Send frame %3" PRId64 "\n", frame->pts);

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
		av_packet_rescale_ts(pkt, c->time_base, video_stream->time_base);
		pkt->stream_index = video_stream->index;

		//printf("Write packet %3" PRId64 " (size=%5d)\n", pkt->pts, pkt->size);
		av_write_frame(output_c, pkt);
		av_packet_unref(pkt);
	}
}

void QVideoWriter::createVideo(QString filename, int width, int height, int fps) {
	video_width = width;
	video_height = height;
	video_fps = fps;

	video_filename = filename;

	//Setup FFmpeg correctly
	int ret;

	output_f = av_guess_format(NULL, filename.toStdString().c_str(), NULL);
	if (!output_f) {
		output_f = av_guess_format("mpeg", NULL, NULL);
	}

	avformat_alloc_output_context2(&output_c, NULL, NULL, filename.toStdString().c_str());
	if(!output_c) {
		throw VideoWriterError("Error allocating format context\n");
	}
	output_c->oformat = output_f;

	QString codec_name = "libx264";
	
	codec = avcodec_find_encoder_by_name(codec_name.toStdString().c_str());
	if (!codec) {
		QString error_msg = QString("Codec %1 not found\n").arg(codec_name);
		throw VideoWriterError( error_msg.toStdString() );
	}
	

	video_stream= avformat_new_stream(output_c, codec);
	if(!video_stream)
	{
		throw VideoWriterError("Could not allocate stream\n");
	}

	pkt = av_packet_alloc();
	if (!pkt)
		throw VideoWriterError( "Could not allocate packet buffer\n");

	c = avcodec_alloc_context3(codec);
	if (!c) {
		throw VideoWriterError( "Could not allocate video codec context\n");
	}
	/* put sample parameters */
	c->bit_rate = 400000;
	/* resolution must be a multiple of two */
	c->width = width;
	c->height = height;
	c->coded_width = width;
	c->coded_height = height;
	/* frames per second */
	c->time_base = (AVRational){1, fps};
	c->framerate = (AVRational){fps, 1};

	/* emit one intra frame every ten frames
	* check frame pict_type before passing frame
	* to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
	* then gop_size is ignored and the output of encoder
	* will always be I frame irrespective to gop_size
	*/
	c->gop_size = 0;
	c->pix_fmt = AV_PIX_FMT_YUV420P;

	av_opt_set(c->priv_data, "crf", "0", 0);

	/* open it */
	ret = avcodec_open2(c, codec, NULL);
	if (ret < 0) {
		QString error_msg = QString("Could not open codec error code %1\n").arg(ret);
		throw VideoWriterError( error_msg.toStdString() );
	}
	avcodec_parameters_from_context(video_stream->codecpar, c);
	video_stream->time_base = (AVRational){1, fps};

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

	if (avio_open(&output_c->pb, filename.toStdString().c_str(), AVIO_FLAG_WRITE) < 0)
	{
		QString error_msg = QString("Could not open file %1\n").arg(filename);
		throw VideoWriterError( error_msg.toStdString() );
	}

	ret = avformat_write_header(output_c, NULL);
	if (ret < 0) {
		throw VideoWriterError( "Error writing file header\n");
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
	img_convert_ctx = sws_getCachedContext(img_convert_ctx, video_width, video_height,AV_PIX_FMT_BGRA, video_width, video_height,AV_PIX_FMT_YUV420P,SWS_BICUBIC, NULL, NULL, NULL);
	if (img_convert_ctx == NULL) {
		throw VideoWriterError("Cannot initialize the conversion context\n");
	}

	uint8_t *srcplanes[3];
	srcplanes[0]=(uint8_t*)frame_image.bits();
	srcplanes[1]=0;
	srcplanes[2]=0;

	int srcstride[3];
	srcstride[0]=frame_image.bytesPerLine();
	srcstride[1]=0;
	srcstride[2]=0;


	sws_scale(img_convert_ctx, srcplanes, srcstride,0, video_height, frame->data, frame->linesize);
	frame->pts = frame_cnt;

	/* encode the image */
	encode_frame(c, frame, pkt, video_file);
	frame_cnt += 1;
}

void QVideoWriter::finish() {
	encode_frame(c, NULL, pkt, video_file);

	av_write_trailer(output_c);

	avcodec_free_context(&c);
	av_frame_free(&frame);
	av_packet_free(&pkt);
	avformat_free_context(output_c);
}

