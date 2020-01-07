#ifndef QVideoWriter_H
#define QVideoWriter_H

#include <QIODevice>
#include <QFile>
#include <QImage>
#include <exception>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>

#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/error.h>
#include <libavutil/rational.h>
#include <libavutil/frame.h>
#include <libavutil/avstring.h>
#include <libavutil/imgutils.h>

#include <libswscale/swscale.h>
}

class VideoWriterError: public std::exception
{
	protected:
		std::string text;
	public:
		VideoWriterError(std::string text): text(text) {}
		virtual const char* what() const noexcept { return text.c_str(); }
};

class QVideoWriter
{
   private:
		QString video_filename;
		unsigned int video_width;
		unsigned int video_height;
		unsigned int video_fps;

		unsigned int frame_cnt;

		AVCodec *codec;
		AVCodecContext *c;
		AVFormatContext *output_c;
		AVOutputFormat *output_f;
		AVStream *video_stream;
		FILE *video_file;
		AVFrame *frame;
		AVPacket *pkt;
		uint8_t endcode[4];
		SwsContext *img_convert_ctx; 

		void encode_frame(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt, FILE *outfile);
   public:
		QVideoWriter();
		void createVideo(QString filename, int width, int height, int fps=25);
		void addFrame(const QImage &frame_image);
		void finish();
};




#endif
