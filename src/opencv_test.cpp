#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "opencv2/imgproc/imgproc.hpp"
#include <pthread.h>
#include <iostream>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib-object.h>
#include <stdint.h>
#include <time.h>
#include <cairo.h>



using namespace cv;
using namespace std;

volatile double average_real, brigthness_real,contrast_real,rotate_real;
volatile uint8_t OPTION_VAR = 0;

#define IMAGE_POSITIVE_NEGATIVE 	0x01
#define IMAGE_MIRROR_UP_DOWN 		0x02
#define IMAGE_MIRROR_LEFT_RIGHT		0x04
#define IMAGE_ROTATE				0x08
#define IMAGE_BRIGTHNESS			0x10
#define IMAGE_CONTRAST				0x20
#define IMAGE_AVERAGE				0x40


static char *fileNameDefaults = "Image_Test.jpeg";
static char *fileSaveLocation = "/home/root";
volatile int iAngle = 180,global_average = 0,global_contrast = 0;
volatile double global_brigthness = 1;

typedef struct _allData{

	GtkWidget *video_window,*icon_draw,*icon_draw_align,*LIVE_INDICATOR_DRAWING_AREA,*LIVE_INDICATOR_DRAWING_AREA_ALIGN;

	GtkWidget *app_window;
	GtkWidget *play_button,*main_box;
	GtkWidget *buttonLive,*buttonPulse,*buttonRotationP,*buttonRotationN,*buttonFilePicker,*buttonImageSave;

	GtkWidget *dialog;

	GtkWidget *main_label;
	GtkWidget *buttonzoom1,*buttonzoom2, *buttonzoom3;
	GtkWidget *buttonMirrorX, *buttonMirrorY;
	GtkWidget *buttonImageP,*buttonImageN;

	GtkWidget *hbox1,*hbox2,*hbox3,*hbox4,*eb;
	GtkWidget *vbox1,*vbox2,*vbox3,*vbox4, *aspectFrameLive,*aspectFrameRecord;


	/* GTK element */
	GtkWidget *label_channel, *label_activity;
	guint bus_watch_id;



	gulong video_window_xid ;

	GtkWidget *Trackbar,*trackbar_average,*trackbar_brigthness,*trackbar_contrast;
	GtkObject *adj,*adj_average,*adj_brigthness,*adj_contrast;
	GtkWidget *emptyBox1,*emptyBox2,*hbox_average,*hbox_brigthness,*hbox_contrast;
	GtkWidget *label_average,*label_average_value,*label_contrast,*label_contrast_value,*label_brigthness,*label_brigthness_value;
	GtkWidget *vbox_average_container,*vbox_brigthness_container,*vbox_contrast_controller;

	GtkWidget *LOGO;


}allData;



Mutex mtx;
volatile bool isCaptured_Sync,shouldCapture,shouldCapture_FirstHit,show_icon_flag = false;
volatile static bool shouldProceed = false;
volatile static int icon_indicator_flag_count = 0;
volatile static bool foot_button_event,foot_button_event_pressed;

volatile bool footSwitch;
bool isFirstRun = true;
unsigned char *buffer;
struct v4l2_buffer buf;

#define WIDTH 720
#define HEIGHT 576


int fd;
Mat CapturedData;
static Mat StaticLocalData,LocalData,ProcessedLocalData,GausianFrame;
static Mat FootSwitchIconData;


static gboolean expose_cb (GtkWidget *da, GdkEvent *event, gpointer data);
//Buttons Callback Functions

static void main_buttonStop_cb(GtkWidget *widget, allData *data);
static void ImageNegative_CallBack(GtkWidget *widget, allData *data);
static void ImageMirrorH_CallBack(GtkWidget *widget, allData *data);
static void ImageMirrorV_CallBack(GtkWidget *widget, allData *data);
static void delete_event_cb (GtkWidget *widget, GdkEvent *event, allData *data);
static void rotation_callback(GtkAdjustment *ADJ_LOC);
static void buttonFilePicker_CallBack(GtkWidget *widget, allData *data);
static void buttonSaveImage_CallBack(GtkWidget *widget, allData *data);
static gboolean icon_draw_expose_cb (GtkWidget *da, GdkEvent *event, gpointer _data);
static void buttonRotationPlus_cb(GtkWidget *widget, allData *data);
static void buttonRotationMinus_cb(GtkWidget *widget, allData *data);
static void buttonFileManagerOpen_cb(GtkWidget *widget, allData *data);
static void  *TEST(void * _data);
static void GPIO_StateChange(bool isHigh);

void gpio_thread_camera_reset(void *data);
void reset_acm(allData *data);

static void average_cb(GtkAdjustment *adj_loc);
static void adj_brigthness_cb(GtkAdjustment *adj_loc);
static void adj_contrast_cb(GtkAdjustment *adj_loc);
static gboolean update_label(gpointer _data);

void *frame_show_thread_work(void *data);
Mat capture_image(int fd);
int capture_image_other(int fd);
int stream_on(int fd);
int init_mmap(int fd);
int print_caps(int fd);
static int xioctl(int fd, int request, void *arg);
void cam_init();

static void CreateUI(allData *data);
GdkAppLaunchContext *context1;
GMainContext *context;

volatile bool shouldStartThrow;

void *gpio_thread_function(void *data){

	//char direction_out[10] = {'o','u','t'};
	char direction_in[10] = {'i','n'};
	//char value_0[1] = {'0'};
	//char value_1[1] = {'1'};
	char read_buf[5] = {0};

	int fd = open( "/sys/class/gpio/gpio14/direction",O_WRONLY);
	if(write(fd,direction_in,2) != 2){
		g_printerr("Couldn't Write Direction\n");
	}

	int fd_value;
	while(1){

		fd_value = open("/sys/class/gpio/gpio14/value",O_RDONLY);
		if(read(fd_value,read_buf,1) != 1){
				g_printerr("Couldn't Read GPIO 14 \n");
		}else{
			g_printerr("Read : %d \n",read_buf[0]);
			if(read_buf[0] == 49){
				footSwitch = true;
				shouldCapture_FirstHit = true;
			}else if(read_buf[0] == 48){
				footSwitch = false;
				if((shouldCapture == false) && shouldCapture_FirstHit){
					shouldCapture_FirstHit = false;

				}
			}
		}
		close(fd_value);

		usleep(100000);
	}
}


static gpointer VideoDisplayThread(gpointer _data)
{
	allData *data = (allData *)_data;
	//VideoWriter FrameWriter;
	//FrameWriter.open("NEW.avi",CV_FOURCC('M','J','P','G'),7,Size(720,576),false);
	//if(!FrameWriter.isOpened()){
		//cout << "Not Open to Make Video" <<endl;
	//}

	sleep(3);

    static GdkDrawable *draw;


    static GdkGC *GC;

    static GdkDrawable *indicatorDraw = gtk_widget_get_window(data->LIVE_INDICATOR_DRAWING_AREA);
	static GdkGC *indicatorGC = gdk_gc_new(GDK_DRAWABLE(draw));

	GdkPixbuf *icon_data = gdk_pixbuf_new_from_file("/home/root/L.jpg",NULL);
	GdkPixbuf *blank_data = gdk_pixbuf_new(GDK_COLORSPACE_RGB,FALSE,8,99,63);
	GdkPixbuf *pix = gdk_pixbuf_new(GDK_COLORSPACE_RGB,FALSE,8,700,700);
	GdkPixbuf *pix1 = gdk_pixbuf_new(GDK_COLORSPACE_RGB,FALSE,8,700,700);

	draw = gtk_widget_get_window(data->video_window);
	GC = gdk_gc_new(GDK_DRAWABLE(draw));

	gdk_threads_enter();

	gdk_draw_gray_image(draw,GC,0,0,StaticLocalData.cols,StaticLocalData.rows,GDK_RGB_DITHER_NONE,(guchar *)StaticLocalData.data,StaticLocalData.step[0]);
	gdk_draw_pixbuf(draw, NULL, pix, 0, 0, 0, 0, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
	gdk_threads_leave();


	Mat matRotation;

	icon_indicator_flag_count = 50;


	char text_rotation[50];
	char text_image_temprature[50];
	char text_mirror_x[50];
	char text_mirror_y[50];

	Mat matOrignal,matResized,matRotated,matNegative,matVMirror,matHMirror,matBrigtness,matContrast,matAverage,matDeinterlaced;

    //OPTION_VAR;
    //iAngle = 0;

	cairo_t *cr;
	int pwidth,pheight,stride;
	unsigned char *data1;
	cairo_surface_t *surface;
	Mat extraRowU,extraRowD,extraColL,extraColR;

	while(true){
		try{



			if(footSwitch && show_icon_flag){
				show_icon_flag = false;
				foot_button_event = true;
				foot_button_event_pressed = true;
			}else if(!footSwitch && !show_icon_flag){
				show_icon_flag = true;
				foot_button_event = true;
				foot_button_event_pressed = false;
			}

			if(foot_button_event){
				foot_button_event = false;
				icon_indicator_flag_count = 50;
			}

			if(icon_indicator_flag_count > 0){
				icon_indicator_flag_count --;
				g_print("Printing : %d",icon_indicator_flag_count);
				if(foot_button_event_pressed){
					//Show Image
					gdk_threads_enter();
					gdk_draw_pixbuf(indicatorDraw, indicatorGC, icon_data , 0, 0, 0, 0, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
					gdk_threads_leave();
				}else{
					//Hide Image
					gdk_threads_enter();
					gdk_draw_pixbuf(indicatorDraw, indicatorGC, blank_data , 0, 0, 0, 0, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
					gdk_threads_leave();
				}
			}

			if(isCaptured_Sync && footSwitch){
				LocalData = CapturedData.clone();
				isCaptured_Sync = false;
				shouldProceed = true;
				//shouldCapture = false;
			}  if(shouldCapture){
				sleep(1);
				shouldCapture = false;
				//char *fullName = "";
				//char *fileName;
				time_t t = time(NULL);
					struct tm _tm = *localtime(&t);

					char filename[200] = {0};
					sprintf(filename,"%s/MEM_%d_%d_%d  %d-%d-%d.jpg",fileSaveLocation,_tm.tm_year+1900,_tm.tm_mon+1,_tm.tm_mday,_tm.tm_hour,_tm.tm_min,_tm.tm_sec);
					g_print(filename);

				imwrite(filename,StaticLocalData);
			}

				if(shouldProceed && shouldStartThrow ){
					shouldProceed = false;
					//OPTION_VAR = 0;


					//This is De-interlacing

					for(int i = 0; i < LocalData.rows - 1; i += 2) {
						LocalData.row(i).copyTo(LocalData.row(i + 1));
					}


					//Image Zoom


					//matOrignal = StaticLocalData(Rect(0,0,576,576)).clone();

					StaticLocalData = LocalData.clone();


					//IMage Rotate

					if(OPTION_VAR & IMAGE_ROTATE){
						//get the affine transformation matrix
						matRotation = getRotationMatrix2D( Point(StaticLocalData.cols / 2, StaticLocalData.rows / 2), (iAngle - 180), 1 );
						//Rotate the image
						warpAffine( StaticLocalData, StaticLocalData, matRotation, StaticLocalData.size());
					}

					//StaticLocalData = StaticLocalData(Rect(0,0,576,576)).clone();

					resize(StaticLocalData,StaticLocalData,Size(700,700),0,0,CV_INTER_CUBIC);


					//IMage Negative
					if(OPTION_VAR & IMAGE_POSITIVE_NEGATIVE){
						Mat new_image = Mat::zeros(StaticLocalData.size(),StaticLocalData.type());
						Mat sub_mat = Mat::ones(StaticLocalData.size(),StaticLocalData.type())*255;
						subtract(sub_mat,StaticLocalData,StaticLocalData);
					}





					//Image Flip up dOWN
					if(OPTION_VAR & IMAGE_MIRROR_UP_DOWN){
						cv::flip(StaticLocalData,StaticLocalData,0);
					}


					//Image Flip Left RIght
					if(OPTION_VAR & IMAGE_MIRROR_LEFT_RIGHT){
						cv::flip(StaticLocalData,StaticLocalData,1);
					}


					//cv::GaussianBlur(StaticLocalData, GausianFrame, cv::Size(0, 0), 10);
					//cv::addWeighted(StaticLocalData, 1.5, GausianFrame, -0.5, 0, StaticLocalData);

					if(OPTION_VAR & IMAGE_AVERAGE){
						/*if(global_average > 0)
						{
							for(int i=1; i<global_average * 2; i += 2){
								blur(StaticLocalData,StaticLocalData,Size(i,i),Point(-1,-1));
							}
						}else
						*/
						if(global_average > 1)
						{

							cv::GaussianBlur(StaticLocalData, GausianFrame, cv::Size(0, 0), (global_average));
							cv::addWeighted(StaticLocalData, 1.5, GausianFrame, -0.5, 0, StaticLocalData);
						}

					}


					if((OPTION_VAR & IMAGE_BRIGTHNESS)){

						//if(global_brigthness > 0)
							StaticLocalData = StaticLocalData + Scalar(global_brigthness,global_brigthness,global_brigthness);
						//else
							//StaticLocalData = StaticLocalData - Scalar(global_brigthness,global_brigthness,global_brigthness);
					}

					if(OPTION_VAR & IMAGE_CONTRAST){
						double x = (double)(global_contrast /50.000);
						fprintf(stderr,"%lf \n",x);
						StaticLocalData.convertTo(StaticLocalData,-1,x,0);
					}



					//

					//----------------------------------Text SHowingmatOrignal = StaticLocalData(Rect(0,0,576,576)).clone(); Area -------------------------------//
					sprintf(text_rotation,"ROTATION : %d",iAngle);

					if(OPTION_VAR & IMAGE_POSITIVE_NEGATIVE){
						sprintf(text_image_temprature,"NEGATIVE");
					}else{
						sprintf(text_image_temprature,"POSITIVE");
					}

					if(OPTION_VAR & IMAGE_MIRROR_UP_DOWN){
						sprintf(text_mirror_y,"MIRR V: YES");
					}else{
						sprintf(text_mirror_y,"MIRR V: NO");
					}

					if(OPTION_VAR & IMAGE_MIRROR_LEFT_RIGHT){
						sprintf(text_mirror_x,"MIRR H: YES");
					}else{
						sprintf(text_mirror_x,"MIRR H: NO");
					}

					//sprintf(text_rotation,"ROTATION : %d",(iAngle - 180));


					putText(StaticLocalData,"LIH",Point(20,40),CV_FONT_HERSHEY_COMPLEX_SMALL,0.8,CV_RGB(255,255,255));
					putText(StaticLocalData,text_rotation,Point(20,60),CV_FONT_HERSHEY_COMPLEX_SMALL,0.8,CV_RGB(255,255,255));
					putText(StaticLocalData,text_image_temprature,Point(20,80),CV_FONT_HERSHEY_COMPLEX_SMALL,0.8,CV_RGB(255,255,255));
					putText(StaticLocalData,text_mirror_y,Point(20,100),CV_FONT_HERSHEY_COMPLEX_SMALL,0.8,CV_RGB(255,255,255));
					putText(StaticLocalData,text_mirror_x,Point(20,120),CV_FONT_HERSHEY_COMPLEX_SMALL,0.8,CV_RGB(255,255,255));

					//--------------------------------------Text Showing Area Ends ---------------------------//


					//


					//in[0] = LocalData;
					//in[1] = LocalData;
					//in[2] = LocalData;
					//cv::merge(in, 3, out);

					//pix = gdk_pixbuf_new_from_data((guchar *)StaticLocalData.data,GDK_COLORSPACE_RGB,false,8, StaticLocalData.cols, StaticLocalData.rows,StaticLocalData.step[0],NULL,NULL);
					draw = gtk_widget_get_window(data->video_window);
					GC = gdk_gc_new(GDK_DRAWABLE(draw));
					matOrignal = Mat::ones(Size(800,800),CV_8UC1);
					matOrignal = StaticLocalData.clone();

					extraRowU = Mat::ones(Size(700,100),CV_8UC1);
					extraRowD = Mat::ones(Size(700,100),CV_8UC1);

					extraColL = Mat::ones(Size(100,800),CV_8UC1);
					extraColR = Mat::ones(Size(100,800),CV_8UC1);

					matResized = StaticLocalData.clone();


					//vconcat(extraRowU,matResized,matResized);
					vconcat(matResized,extraRowU,matResized);

					//hconcat(extraColL,matResized,matResized);
					hconcat(matResized,extraColL,matOrignal);

					//---------------Cairo Thing Starts Here ------------------//
					//surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,700,700);
					//cr = cairo_create(surface);

					//gdk_cairo_set_source_pixbuf(cr,pix,0,0);

					//cairo_paint(cr);

					fprintf(stderr,"Height : %d\nWidth : %d\n",matOrignal.rows,matOrignal.cols);
					gdk_threads_enter();

					//data1 = cairo_image_surface_get_data(surface);
					//pwidth = cairo_image_surface_get_width(surface);
					//pheight = cairo_image_surface_get_height(surface);
					//stride = cairo_image_surface_get_stride(surface);

					//pix1 = gdk_pixbuf_new_from_data((guchar *)data1,GDK_COLORSPACE_RGB,false,8,pwidth,pheight,stride,NULL,NULL);
					//matOrignal = Mat(Size(700, 700), CV_8UC1, data1).clone();
					//gdk_window_clear_area(draw,0,0,700,700);
					//usleep(100000);
					gdk_draw_gray_image(draw,GC,0,0,matOrignal.rows,matOrignal.cols,GDK_RGB_DITHER_NONE,(guchar *)matOrignal.data,matOrignal.step[0]);
					//gdk_draw_pixbuf(draw, NULL, pix1, 0, 0, 0, 0, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
					gdk_threads_leave();

					//isCaptured_Sync = false;

				}


		}catch(Exception *ex){
			printf(ex->what());
		}
	}


    return G_SOURCE_REMOVE;
}

static gpointer FootswitchIconHandler(gpointer _data)
{

	allData *data = (allData *)_data;
	GError *error;
	GdkPixbuf *icon_data,*blank_data;
	GdkDrawable *draw;
	GdkGC *GC;
	icon_data = gdk_pixbuf_new_from_file("/home/root/L.jpg",NULL);
	blank_data = gdk_pixbuf_new(GDK_COLORSPACE_RGB,FALSE,8,100,100);

	while(true){

		draw = gtk_widget_get_window(data->LIVE_INDICATOR_DRAWING_AREA);
		GC = gdk_gc_new(GDK_DRAWABLE(draw));

		if(footSwitch){

			gdk_threads_enter();
			gdk_draw_pixbuf(draw, GC, icon_data , 0, 0, 0, 0, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
			gdk_threads_leave();
		}else{

			gdk_threads_enter();
			gdk_draw_pixbuf(draw, GC, blank_data , 0, 0, 0, 0, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
			gdk_threads_leave();
		}
		usleep(50000);
	}
	return G_SOURCE_REMOVE;
}

static void realize_cb (GtkWidget *widget, gpointer data) {


}


static void InitGPIO_186_187(){


	//Pin 138 136
	char pin_187[10] = {'1','8','7'};
	char pin_186[10] = {'1','8','6'};
	ssize_t bytes_written;
	int fd;

	fd = open("/sys/class/gpio/export", O_WRONLY);


	if (-1 == fd) {
		fprintf(stderr, "Failed to open export for writing!\n");
		return;
	}

	write(fd, pin_187, 3);
	write(fd, pin_186, 3);
	close(fd);

}

static void GPIO_StateChange(bool isHigh){
	char direction_out[10] = {'o','u','t'};
	char value_0[1] = {'0'};
	char value_1[1] = {'1'};

	int fd186 = open( "/sys/class/gpio/cd/direction",O_WRONLY);
	if(write(fd186,direction_out,3) != 3){
		g_printerr("Couldn't Write Direction\n");
	}
	close(fd186);

	int fd187 = open( "/sys/class/gpio/gpio187/direction",O_WRONLY);
	if(write(fd187,direction_out,3) != 3){
		g_printerr("Couldn't Write Direction\n");
	}
	close(fd187);



	int fd_value_186 = open( "/sys/class/gpio/gpio186/value",O_WRONLY);
	int fd_value_187 = open( "/sys/class/gpio/gpio187/value",O_WRONLY);

	if(isHigh){
		if(write(fd_value_186,value_1,1) != 1){
			g_printerr("Couldn't Write Value 1 to GPIO 186\n");
		}

		if(write(fd_value_187,value_1,1) != 1){
			g_printerr("Couldn't Write Value 1 to GPIO 187\n");
		}
	}else{
		if(write(fd_value_186,value_0,1) != 1){
			g_printerr("Couldn't Write Value 0 to GPIO 186\n");
		}

		if(write(fd_value_187,value_0,1) != 1){
			g_printerr("Couldn't Write Value 0 to GPIO 187\n");
		}
	}


	close(fd_value_186);
	close(fd_value_187);
}

allData data;

int main()
{


    XInitThreads();
	gtk_init (NULL, NULL);

	//InitGPIO_186_187();

	Mat d(720,800,DataType<float>::type);
	Mat d_c(720,576,DataType<float>::type);
	CapturedData = d_c.clone();
	StaticLocalData = d.clone();
	LocalData = d.clone();
	FootSwitchIconData = d.clone();

	context = g_main_context_default();
	//context1 = g_app_launch_context_new();
	isCaptured_Sync = false;
	footSwitch = false;
	shouldStartThrow = false;
	shouldCapture = false;
	shouldCapture_FirstHit = false;





	gpio_thread_camera_reset(NULL);

	pthread_t  frame_thread;
	pthread_create(&frame_thread,NULL,frame_show_thread_work,NULL);


	pthread_t  gpio_thread;
	pthread_create(&gpio_thread,NULL,gpio_thread_function,NULL);

	CreateUI(&data);

	g_thread_new("video", VideoDisplayThread,&data);
	//g_thread_new("footSwitchIcon", FootswitchIconHandler,&data);

	//gdk_threads_enter();
	shouldStartThrow = true;
    gtk_main ();
    //gdk_threads_leave();

    return 0;


}


void cam_init(){
	fd = open("/dev/video0", O_RDWR);
	print_caps(fd);
	init_mmap(fd);
}


void *frame_show_thread_work(void *data){

    fd = open("/dev/video0", O_RDWR);
    if (fd == -1)
    {
            printf("Couldn't Open Camera  \n");
    }

    if(print_caps(fd))
    	printf("Couldn't Set Camera Parameters \n");


    if(init_mmap(fd))
    	printf("Couldn't Init Memory \n");


	stream_on(fd);

    while(true){
			if(!isCaptured_Sync){
				capture_image_other(fd);
				CapturedData = Mat(Size(720, 576), CV_8UC1, buffer).clone();

				//if(!capture_image_other(fd)){
					//CapturedData = Mat(Size(720, 576), CV_8UC1, buffer).clone();
				//}


				isCaptured_Sync  = true;

			}

    }
}


static int xioctl(int fd, int request, void *arg)
{
        int r;

        do r = ioctl (fd, request, arg);
        while (-1 == r && EINTR == errno);

        return r;
}

int print_caps(int fd)
{
        struct v4l2_capability caps = {};
        if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &caps))
        {
                perror("Querying Capabilities");
                return 1;
        }

        printf( "Driver Caps:\n"
                "  Driver: \"%s\"\n"
                "  Card: \"%s\"\n"
                "  Bus: \"%s\"\n"
                "  Version: %d.%d\n"
                "  Capabilities: %08x\n",
                caps.driver,
                caps.card,
                caps.bus_info,
                (caps.version>>16)&&0xff,
                (caps.version>>24)&&0xff,
                caps.capabilities);



        char fourcc[5] = {0};


        struct v4l2_format fmt = {0};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = 720;
        fmt.fmt.pix.height = 576;
        //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
        //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YVU420;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;

        if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
        {
            perror("Setting Pixel Format");
            return 1;
        }

        strncpy(fourcc, (char *)&fmt.fmt.pix.pixelformat, 4);
        printf( "Selected Camera Mode:\n"
                "  Width: %d\n"
                "  Height: %d\n"
                "  PixFmt: %s\n"
                "  Field: %d\n",
                fmt.fmt.pix.width,
                fmt.fmt.pix.height,
                fourcc,
                fmt.fmt.pix.field);
        return 0;
}

int init_mmap(int fd)
{
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req))
    {
        perror("Requesting Buffer");
        return 1;
    }

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
    {
        perror("Querying Buffer");
        return 1;
    }


    buffer = (unsigned char*)mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

    printf("Length: %d\nAddress: %p\n", buf.length, buffer);
    printf("Image Length: %d\n", buf.bytesused);

    return 0;
}

Mat capture_image(int fd)
{




    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    xioctl(fd, VIDIOC_QBUF, &buf);
    if(isFirstRun){
    	isFirstRun = false;
		xioctl(fd, VIDIOC_STREAMON, &buf.type);
    }
    usleep(10);
    xioctl(fd, VIDIOC_DQBUF, &buf);
    Mat image(Size(720, 576), CV_8UC1, buffer);
    return image;
}

int capture_image_other(int fd)
{

    if(xioctl(fd, VIDIOC_DQBUF, &buf))
    	return 1;

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if(xioctl(fd, VIDIOC_QBUF, &buf))
    	return 1;

    usleep(500);

    return 0;
}

int stream_on(int fd){

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if(xioctl(fd, VIDIOC_QBUF, &buf))
    	return 1;

	if(xioctl(fd, VIDIOC_STREAMON, &buf.type))
		return 1;

    return 0;
}





static void CreateUI(allData *data){



	GtkWidget *buttonPlay,*buttonPause, *buttonStop, *buttonRecordStart, *buttonRecordStop, *buttonCapture;
	GtkWidget *alignTrackBar,*alignVideoFRame,*alignVBoxFirst,*alignVBoxSecond;
	GtkWidget *buttonRotationPlus,*buttonRotationMinus;
	GtkWidget *buttonFileManagerOpen;

	gint button_height = 60 ,button_width = 120;

	//data->buttonLive = gtk_button_new_with_label("LIVE");
	//g_signal_connect (data->buttonLive , "clicked",G_CALLBACK (main_buttonlive1_cb), data);

	//data->buttonPulse = gtk_button_new_with_label("PULSE");
	//g_signal_connect (data->buttonPulse , "clicked",G_CALLBACK (main_buttonPulse_cb), data);

	//data->buttonRotationP= gtk_button_new_with_label("Rotate +");
	//data->buttonRotationN = gtk_button_new_with_label("Rotate -");

	//buttonPlay = gtk_button_new_with_label("LIH");
	//g_signal_connect (buttonPlay, "clicked",G_CALLBACK (main_buttonPlay_cb), data);

	//buttonPause = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PAUSE);
	//g_signal_connect (buttonPause, "clicked",G_CALLBACK (main_buttonPause_cb), data);


	GtkWidget *main_control_box_v = gtk_vbox_new(false,3);
	//gtk_widget_set_size_request(main_control_box_v,250,700);

	GtkWidget *control_holder_box_h = gtk_hbox_new(false,3);


	buttonStop = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	g_signal_connect (buttonStop, "clicked",G_CALLBACK (main_buttonStop_cb), data);
	gtk_widget_set_size_request(buttonStop,button_width,button_height);


	//buttonRecordStart = gtk_button_new_with_label("Cina Loop");
	//g_signal_connect (buttonRecordStart, "clicked",G_CALLBACK (main_buttonRecordStart_cb), data);

	//buttonRecordStop = gtk_button_new_with_label("STOP REC");
	//g_signal_connect (buttonRecordStop, "clicked",G_CALLBACK (main_buttonRecordStop_cb), data);

	//buttonCapture = gtk_button_new_with_label("CAPTURE");
	//g_signal_connect (buttonCapture, "clicked",G_CALLBACK (main_buttonCapture_cb), data);

	//data->buttonImageP = gtk_button_new_with_label("Image +");
	data->buttonImageN = gtk_button_new_with_label("Negative");
	g_signal_connect (data->buttonImageN, "clicked",G_CALLBACK (ImageNegative_CallBack), data);
	gtk_widget_set_size_request(data->buttonImageN,button_width,button_height);

	data->buttonMirrorX = gtk_button_new_with_label("Mirror H");
	g_signal_connect (data->buttonMirrorX, "clicked",G_CALLBACK (ImageMirrorH_CallBack), data);
	gtk_widget_set_size_request(data->buttonMirrorX,button_width,button_height);

	data->buttonMirrorY = gtk_button_new_with_label("Mirror V");
	g_signal_connect (data->buttonMirrorY, "clicked",G_CALLBACK (ImageMirrorV_CallBack), data);
	gtk_widget_set_size_request(data->buttonMirrorY,button_width,button_height);

	data->buttonFilePicker = gtk_button_new_with_label("Set Save Location");
	g_signal_connect (data->buttonFilePicker, "clicked",G_CALLBACK (buttonFilePicker_CallBack), data);
	gtk_widget_set_size_request(data->buttonFilePicker,button_width,button_height);

	data->buttonImageSave = gtk_button_new_with_label("Save");
	g_signal_connect (data->buttonImageSave, "clicked",G_CALLBACK (buttonSaveImage_CallBack), data);
	gtk_widget_set_size_request(data->buttonImageSave,button_width,button_height);

	buttonRotationPlus = gtk_button_new_with_label("+");
	g_signal_connect (buttonRotationPlus, "clicked",G_CALLBACK (buttonRotationPlus_cb), data);
	gtk_widget_set_size_request(buttonRotationPlus,40,20);


	buttonRotationMinus = gtk_button_new_with_label("-");
	g_signal_connect (buttonRotationMinus, "clicked",G_CALLBACK (buttonRotationMinus_cb), data);
	gtk_widget_set_size_request(buttonRotationMinus,40,20);

	buttonFileManagerOpen = gtk_button_new_with_label("Files");
	g_signal_connect (buttonFileManagerOpen, "clicked",G_CALLBACK (buttonFileManagerOpen_cb), data);
	gtk_widget_set_size_request(buttonFileManagerOpen,button_width,button_height);

	//data->buttonzoom1 = gtk_button_new_with_label("Zoom 1x");
	//data->buttonzoom2 = gtk_button_new_with_label("Zoom 2x");
	//data->buttonzoom3 = gtk_button_new_with_label("Zoom 3x");




	data->adj = gtk_adjustment_new(180,0,359,1,1,0);
	g_signal_connect(data->adj,"value_changed",G_CALLBACK(rotation_callback),NULL);
	data->Trackbar = gtk_hscale_new(GTK_ADJUSTMENT(data->adj));
	gtk_widget_set_size_request(data->Trackbar,400,30);




	//Box with Button in Column1
	data->vbox1 = gtk_vbox_new(FALSE,3);
	//gtk_box_pack_start(GTK_BOX(data->vbox1),buttonPlay,FALSE,FALSE,5);
	//gtk_box_pack_start(GTK_BOX(data->vbox1),buttonRecordStart,FALSE,FALSE,5);
	//gtk_box_pack_start(GTK_BOX(data->vbox1),data->buttonLive,FALSE,FALSE,5);
	//gtk_box_pack_start(GTK_BOX(data->vbox1),data->buttonPulse,FALSE,FALSE,5);
	//gtk_box_pack_start(GTK_BOX(data->vbox1),buttonStop,FALSE,FALSE,5);

	//alignVBoxFirst = gtk_alignment_new(0,0,0,0);
	//gtk_container_add(GTK_CONTAINER(alignVBoxFirst),data->vbox1);
	//gtk_alignment_set(GTK_ALIGNMENT(alignVBoxFirst),1,1,0,0);

	//Box with Button in Column2
	data->vbox2 = gtk_vbox_new(FALSE,3);
	GtkWidget *vboxSecondCol = gtk_vbox_new(FALSE,3);
	//gtk_box_pack_start(GTK_BOX(data->vbox2),data->buttonImageP,FALSE,FALSE,5);
	gtk_box_pack_start(GTK_BOX(data->vbox2),data->buttonFilePicker,FALSE,FALSE,5);
	gtk_box_pack_start(GTK_BOX(data->vbox2),data->buttonImageSave,FALSE,FALSE,5);

	gtk_box_pack_start(GTK_BOX(data->vbox2),data->buttonImageN,FALSE,FALSE,5);
	//gtk_box_pack_start(GTK_BOX(data->vbox2),buttonFileManagerOpen,FALSE,FALSE,5);


	gtk_box_pack_start(GTK_BOX(vboxSecondCol),data->buttonMirrorX,FALSE,FALSE,5);
	gtk_box_pack_start(GTK_BOX(vboxSecondCol),data->buttonMirrorY,FALSE,FALSE,5);
	//gtk_box_pack_start(GTK_BOX(data->vbox2),data->buttonzoom1,FALSE,FALSE,5);
	//gtk_box_pack_start(GTK_BOX(data->vbox2),data->buttonzoom2,FALSE,FALSE,5);
	//gtk_box_pack_start(GTK_BOX(data->vbox2),data->buttonzoom3,FALSE,FALSE,5);
	//gtk_box_pack_start(GTK_BOX(data->vbox2),data->buttonRotationP,FALSE,FALSE,5);
	//gtk_box_pack_start(GTK_BOX(data->vbox2),data->buttonRotationN,FALSE,FALSE,5);
	gtk_box_pack_start(GTK_BOX(vboxSecondCol),buttonStop,FALSE,FALSE,5);

	gtk_box_pack_start(GTK_BOX(control_holder_box_h),data->vbox2,false,false,5);
	gtk_box_pack_start(GTK_BOX(control_holder_box_h),vboxSecondCol,false,false,5);


	data->vbox4 = gtk_vbox_new(FALSE,3);


	data->label_activity = gtk_label_new("CONTROLS ");
	GtkWidget *labelactivityalign = gtk_alignment_new(0,0,0,0);
	gtk_container_add(GTK_CONTAINER(labelactivityalign),data->label_activity);
	gtk_alignment_set(GTK_ALIGNMENT(labelactivityalign),0.5,0.4,0,0);


	data->LOGO = gtk_label_new("MEM");
	//gtk_label_set_angle(GTK_LABEL(LOGO),90);
	gtk_label_set_markup(GTK_LABEL(data->LOGO),"<span foreground=\"blue\" font_family=\"URW Chancery L\" font=\"60\" weight=\"bold\">X</span><span foreground=\"blue\" font_family=\"URW Chancery L\" font=\"35\" weight=\"bold\">MEM-966</span>");
	//gtk_box_pack_start(GTK_BOX(data->vbox1),LOGO,FALSE,FALSE,5);
	GtkWidget *LOGOalign = gtk_alignment_new(0,0,0,0);
	gtk_container_add(GTK_CONTAINER(LOGOalign),data->LOGO);
	gtk_alignment_set(GTK_ALIGNMENT(LOGOalign),0.5,0.2,0,0);


	data->hbox3 = gtk_hbox_new(TRUE,5);


	//alignVBoxSecond = gtk_alignment_new(0,0,0,0);
	//gtk_container_add(GTK_CONTAINER(alignVBoxSecond),LOGO);
	//gtk_alignment_set(GTK_ALIGNMENT(alignVBoxSecond),0.5,0.5,0,0);

	//gtk_box_pack_start(GTK_BOX(data->vbox1),alignVBoxSecond,FALSE,FALSE,20);
	//gtk_box_pack_start(GTK_BOX(alignVBoxSecond),data->vbox2,FALSE,FALSE,10);


	data->vbox3 = gtk_vbox_new(FALSE,5);
	//gtk_box_pack_start(GTK_BOX(data->vbox3),data->label_activity,FALSE,FALSE,5);
	//gtk_box_pack_start(GTK_BOX(data->vbox3),data->vbox2,FALSE,FALSE,5);

	//GtkWidget *alignControlsBoxFirst = gtk_alignment_new(0,0,0,0);
	//gtk_container_add(GTK_CONTAINER(alignControlsBoxFirst),data->vbox3);
	//gtk_alignment_set(GTK_ALIGNMENT(alignControlsBoxFirst),0.5,0.5,0,0);

	//GtkWidget *alignControlsBoxSecond = gtk_alignment_new(0,0,0,0);
	//gtk_container_add(GTK_CONTAINER(alignControlsBoxSecond),vboxSecondCol);
	//gtk_alignment_set(GTK_ALIGNMENT(alignControlsBoxSecond),0.5,0.5,0,0);

	GtkWidget *labelCopyright = gtk_label_new("Copyright Reserved @ Simtronics");
	gtk_label_set_markup(GTK_LABEL(labelCopyright),"<span foreground=\"white\" font_family=\"URW Chancery L\" font=\"12\" weight=\"bold\">Copyright Reserved @ Simtronics</span>");
	GtkWidget *labelCopyrightalign = gtk_alignment_new(0,0,0,0);
	gtk_container_add(GTK_CONTAINER(labelCopyrightalign),labelCopyright);
	gtk_alignment_set(GTK_ALIGNMENT(labelCopyrightalign),0.5,1,0,0);


	data->emptyBox1 = gtk_vbox_new(false,2);

	data->emptyBox2 = gtk_vbox_new(false,2);
	gtk_widget_set_size_request(data->emptyBox2,100,60);




	//---------------averaging trackbar ------------------------------//
	data->adj_average = gtk_adjustment_new(1,1,30,1,1,0);
	g_signal_connect(data->adj_average,"value_changed",G_CALLBACK(average_cb),NULL);
	data->trackbar_average = gtk_hscale_new(GTK_ADJUSTMENT(data->adj_average));
	gtk_widget_set_size_request(data->trackbar_average,160,30);

	//---------------brigthness trackbar ------------------------------//
	data->adj_brigthness = gtk_adjustment_new(1,-99,99,1,1,0);
	g_signal_connect(data->adj_brigthness,"value_changed",G_CALLBACK(adj_brigthness_cb),NULL);
	data->trackbar_brigthness = gtk_hscale_new(GTK_ADJUSTMENT(data->adj_brigthness));
	gtk_widget_set_size_request(data->trackbar_brigthness,160,30);

	//---------------contrast trackbar ------------------------------//
	data->adj_contrast = gtk_adjustment_new(50,1,100,1,1,0);
	g_signal_connect(data->adj_contrast,"value_changed",G_CALLBACK(adj_contrast_cb),NULL);
	data->trackbar_contrast = gtk_hscale_new(GTK_ADJUSTMENT(data->adj_contrast));
	gtk_widget_set_size_request(data->trackbar_contrast,160,30);


	data->hbox_average = gtk_hbox_new(false,0);
	gtk_widget_set_size_request(data->hbox_average,250,30);

	data->hbox_brigthness = gtk_hbox_new(false,0);
	gtk_widget_set_size_request(data->hbox_brigthness,250,30);

	data->hbox_contrast = gtk_hbox_new(false,0);
	gtk_widget_set_size_request(data->hbox_contrast,250,30);


	data->label_average = gtk_label_new("AVERAGE:   ");
	gtk_label_set_markup(GTK_LABEL(data->label_average),"<span foreground=\"white\"  font=\"8\">AVERAGE:     </span>");
	data->label_average_value = gtk_label_new("AVERAGE:   ");
	gtk_label_set_markup(GTK_LABEL(data->label_average_value),"<span foreground=\"white\"  font=\"8\">1</span>");
	global_average = 1;

	data->label_brigthness = gtk_label_new("BRIGTHNESS:");
	gtk_label_set_markup(GTK_LABEL(data->label_brigthness),"<span foreground=\"white\"  font=\"8\">BRIGTHNESS: </span>");
	data->label_brigthness_value = gtk_label_new("AVERAGE:   ");
	gtk_label_set_markup(GTK_LABEL(data->label_brigthness_value),"<span foreground=\"white\"  font=\"8\">50%</span>");
	global_brigthness = 1;

	data->label_contrast = gtk_label_new("CONTRAST:  ");
	gtk_label_set_markup(GTK_LABEL(data->label_contrast),"<span foreground=\"white\"  font=\"8\">CONTRAST:    </span>");
	data->label_contrast_value = gtk_label_new("CONTRAST:  ");
	gtk_label_set_markup(GTK_LABEL(data->label_contrast_value),"<span foreground=\"white\"  font=\"8\">50%</span>");
	global_contrast = 50;


	gtk_box_pack_start(GTK_BOX(data->hbox_average),data->label_average,false,false,3);
	gtk_box_pack_start(GTK_BOX(data->hbox_average),data->trackbar_average,false,false,3);
	gtk_box_pack_start(GTK_BOX(data->hbox_average),data->label_average_value,false,false,3);

	gtk_box_pack_start(GTK_BOX(data->hbox_brigthness),data->label_brigthness,false,false,3);
	gtk_box_pack_start(GTK_BOX(data->hbox_brigthness),data->trackbar_brigthness,false,false,3);
	gtk_box_pack_start(GTK_BOX(data->hbox_brigthness),data->label_brigthness_value,false,false,3);

	gtk_box_pack_start(GTK_BOX(data->hbox_contrast),data->label_contrast,false,false,3);
	gtk_box_pack_start(GTK_BOX(data->hbox_contrast),data->trackbar_contrast,false,false,3);
	gtk_box_pack_start(GTK_BOX(data->hbox_contrast),data->label_contrast_value,false,false,3);



	gtk_box_pack_start(GTK_BOX(data->emptyBox1),data->hbox_average,false,false,3);
	gtk_box_pack_start(GTK_BOX(data->emptyBox1),data->hbox_brigthness,false,false,3);
	gtk_box_pack_start(GTK_BOX(data->emptyBox1),data->hbox_contrast,false,false,3);





	data->icon_draw = gtk_image_new_from_file("/home/root/rot.png");
	data->icon_draw_align = gtk_alignment_new(0,0,0,0);
	gtk_container_add(GTK_CONTAINER(data->icon_draw_align),data->icon_draw);
	gtk_alignment_set(GTK_ALIGNMENT(data->icon_draw_align),0.5,0.5,0,0);


	/**************************GTK LIVE IDICATOR***************************/
	data->LIVE_INDICATOR_DRAWING_AREA = gtk_drawing_area_new ();
	gtk_widget_set_size_request(data->LIVE_INDICATOR_DRAWING_AREA,99,63);
	data->LIVE_INDICATOR_DRAWING_AREA_ALIGN = gtk_alignment_new(0,0,0,0);
	gtk_container_add(GTK_CONTAINER(data->LIVE_INDICATOR_DRAWING_AREA_ALIGN),data->LIVE_INDICATOR_DRAWING_AREA);
	gtk_alignment_set(GTK_ALIGNMENT(data->LIVE_INDICATOR_DRAWING_AREA_ALIGN),0.5,0.5,0,0);


	//gtk_widget_set_visible(data->icon_draw,false);

	//g_signal_connect (data->video_window, "expose_event", G_CALLBACK (icon_draw_expose_cb), data);



	//gtk_box_pack_start(GTK_BOX(data->emptyBox1),data->icon_draw_align,false,false,5);


	gtk_box_pack_start(GTK_BOX(main_control_box_v),LOGOalign,false,false,5);
	gtk_box_pack_start(GTK_BOX(main_control_box_v),data->LIVE_INDICATOR_DRAWING_AREA_ALIGN,false,false,5);
	gtk_box_pack_start(GTK_BOX(main_control_box_v),labelactivityalign,false,false,5);
	gtk_box_pack_start(GTK_BOX(main_control_box_v),control_holder_box_h,false,false,5);

	gtk_box_pack_start(GTK_BOX(main_control_box_v),data->emptyBox1,false,false,5);
	gtk_box_pack_start(GTK_BOX(main_control_box_v),data->emptyBox2,false,false,5);
	gtk_box_pack_start(GTK_BOX(main_control_box_v),data->icon_draw_align,false,false,5);
	gtk_box_pack_start(GTK_BOX(main_control_box_v),labelCopyrightalign,false,false,5);



	data->app_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	g_signal_connect (G_OBJECT (data->app_window), "delete-event", G_CALLBACK (delete_event_cb), data);
	g_signal_connect(G_OBJECT(data->app_window),"destroy",G_CALLBACK (delete_event_cb), data);
	gtk_window_set_title(GTK_WINDOW(data->app_window),"MEMORY");

	data->video_window = gtk_drawing_area_new ();
	//g_signal_connect (data->video_window, "realize",G_CALLBACK (video_widget_realize_cb), data);
	g_signal_connect (data->video_window, "expose_event", G_CALLBACK (expose_cb), data);

	//data->video_window_record = gtk_drawing_area_new();
	//g_signal_connect (data->video_window_record, "realize",G_CALLBACK (video_widget_realize_cb), data);
	//g_signal_connect (data->video_window_record, "expose_event", G_CALLBACK (expose_cb), data);

	data->aspectFrameLive = gtk_aspect_frame_new("",0.5,0.5,1,false);



	data->eb = gtk_event_box_new();
	data->hbox2 = gtk_hbox_new(false,0);
	data->hbox1 = gtk_hbox_new(false,0);

	GdkColor color;
	gdk_color_parse ("black", &color);
	gtk_widget_modify_bg(data->app_window,GTK_STATE_NORMAL,&color);
	gdk_color_parse ("black", &color);
	gtk_widget_modify_bg(data->aspectFrameLive,GTK_STATE_NORMAL,&color);



	//gtk_container_add(GTK_CONTAINER(data->eb),data->hbox2);
	gtk_container_add(GTK_CONTAINER(data->aspectFrameLive),data->video_window);


	GtkWidget *RotationLabel = gtk_label_new("Rotation :    ");
	GtkWidget *trackbar_holder_h = gtk_hbox_new(false,5);
	gtk_box_pack_start(GTK_BOX(trackbar_holder_h),RotationLabel,false,false,1);
	gtk_box_pack_start(GTK_BOX(trackbar_holder_h),buttonRotationMinus,false,false,1);
	gtk_box_pack_start(GTK_BOX(trackbar_holder_h),data->Trackbar,false,false,1);
	gtk_box_pack_start(GTK_BOX(trackbar_holder_h),buttonRotationPlus,false,false,1);

	alignTrackBar = gtk_alignment_new(0,0,0,0);
	gtk_container_add(GTK_CONTAINER(alignTrackBar),trackbar_holder_h);
	gtk_alignment_set(GTK_ALIGNMENT(alignTrackBar),0.5,0,0,0);


	gtk_box_pack_start(GTK_BOX(data->vbox4),data->aspectFrameLive,false,false,0);
	gtk_box_pack_start(GTK_BOX(data->vbox4),alignTrackBar,false,false,1);


	gtk_widget_set_size_request(data->aspectFrameLive,700,700);
	gtk_box_pack_start(GTK_BOX(data->hbox1),data->vbox4,false,false,0);
	gtk_box_pack_start(GTK_BOX(data->hbox1),main_control_box_v,false,false,5);
	//gtk_box_pack_start(GTK_BOX(data->hbox1),alignControlsBoxSecond,false,false,30);


	//gtk_window_set_default_size (GTK_WINDOW (data->app_window), 1000, 700);
	gtk_window_fullscreen(GTK_WINDOW(data->app_window));
	gtk_window_set_resizable(GTK_WINDOW(data->app_window),true);
	gtk_container_add (GTK_CONTAINER (data->app_window), data->hbox1);

	gtk_widget_show_all (data->app_window);
	gtk_widget_realize (data->video_window);

}




static void rotation_callback(GtkAdjustment *adj_loc){

	OPTION_VAR |= IMAGE_ROTATE;
	iAngle = (int)gtk_adjustment_get_value(adj_loc);

	//g_print("Rotate : %d \n",iAngle);
	shouldProceed = true;
}


static void average_cb(GtkAdjustment *adj_loc){
	OPTION_VAR |= IMAGE_AVERAGE;
	global_average  = (int)gtk_adjustment_get_value(adj_loc);
	g_main_context_invoke(NULL,update_label,&data);

	shouldProceed = true;
}
static void adj_brigthness_cb(GtkAdjustment *adj_loc){
	OPTION_VAR |= IMAGE_BRIGTHNESS;
	global_brigthness = gtk_adjustment_get_value(adj_loc);
	g_main_context_invoke(NULL,update_label,&data);
	shouldProceed = true;

}
static void adj_contrast_cb(GtkAdjustment *adj_loc)
{
	OPTION_VAR |= IMAGE_CONTRAST;
	global_contrast = gtk_adjustment_get_value(adj_loc);
	g_main_context_invoke(NULL,update_label,&data);
	shouldProceed = true;

}

static gboolean update_label(gpointer _data){

	allData *_cdata = (allData *)_data;


	gchar text_average[200];
	gchar text_brightness[200];
	gchar text_contrast[200];
	gchar text_rotation[200];

	sprintf(text_average,"<span foreground=\"white\"  font=\"8\">%d</span>",global_average);
	sprintf(text_brightness,"<span foreground=\"white\"  font=\"8\">%d%</span>",(int)(((global_brigthness + 100)/2) + 1));
	sprintf(text_contrast,"<span foreground=\"white\"  font=\"8\">%d%</span>",global_contrast);
	//sprintf(text_rotation,"<span foreground=\"white\"  font=\"8\">%d%</span>",rotate_real);


	gtk_label_set_markup(GTK_LABEL(_cdata->label_average_value),text_average);
	gtk_label_set_markup(GTK_LABEL(_cdata->label_brigthness_value),text_brightness);
	gtk_label_set_markup(GTK_LABEL(_cdata->label_contrast_value),text_contrast);
	//gtk_label_set_markup(GTK_LABEL(_cdata->label_average_value),text_average);

	return G_SOURCE_REMOVE;
}


static void delete_event_cb(GtkWidget *widget, GdkEvent *event, allData *data) {
	  printf("Delete EVent\n");
}


static void ImageNegative_CallBack(GtkWidget *widget, allData *data){
	g_print("image mirror Negative \n");
	OPTION_VAR ^= IMAGE_POSITIVE_NEGATIVE;
	shouldProceed = true;
}


static void ImageMirrorH_CallBack(GtkWidget *widget, allData *data){
	g_print("image mirror left right \n");
	OPTION_VAR ^= IMAGE_MIRROR_LEFT_RIGHT;
	shouldProceed = true;
}

static void main_buttonStop_cb(GtkWidget *widget, allData *data){

	printf("NOt Valid");
	system("poweroff");
	gtk_main_quit();
}

static void ImageMirrorV_CallBack(GtkWidget *widget, allData *data){
	g_print("image mirror up down \n");
	OPTION_VAR ^= IMAGE_MIRROR_UP_DOWN;
	shouldProceed = true;
}


static void buttonFilePicker_CallBack(GtkWidget *widget, allData *data){
	data->dialog = gtk_file_chooser_dialog_new("Select Image Save Location",GTK_WINDOW(data->app_window),GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_SAVE,GTK_RESPONSE_ACCEPT,NULL);
	if(gtk_dialog_run(GTK_DIALOG(data->dialog)) == GTK_RESPONSE_ACCEPT){
		fileSaveLocation = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(data->dialog));
	}
	gtk_widget_destroy(data->dialog);
}

static void buttonSaveImage_CallBack(GtkWidget *widget, allData *data){
	shouldCapture = true;
}

static void buttonRotationPlus_cb(GtkWidget *widget, allData *data){

	OPTION_VAR |= IMAGE_ROTATE;
	iAngle = (int)gtk_adjustment_get_value(GTK_ADJUSTMENT(data->adj));
	if(iAngle < 359)
		iAngle += 1;
	gtk_adjustment_set_value(GTK_ADJUSTMENT(data->adj),iAngle);

	g_print("Rotate : %d \n",iAngle);
	shouldProceed = true;
}

static void buttonRotationMinus_cb(GtkWidget *widget, allData *data){
	OPTION_VAR |= IMAGE_ROTATE;
	iAngle = (int)gtk_adjustment_get_value(GTK_ADJUSTMENT(data->adj));
	if(iAngle > 0)
		iAngle -= 1;
	gtk_adjustment_set_value(GTK_ADJUSTMENT(data->adj),iAngle);

	g_print("Rotate : %d \n",iAngle);
	shouldProceed = true;
}

static gboolean expose_cb (GtkWidget *da, GdkEvent *event, gpointer _data)
{

    allData *data = (allData *)_data;



    GError *err = NULL;
    GdkDrawable *draw;
    GdkGC *GC;


	draw = gtk_widget_get_window(data->video_window);
	GC = gdk_gc_new(GDK_DRAWABLE(draw));

	gdk_threads_enter();
	gdk_draw_gray_image(draw,GC,0,0,StaticLocalData.cols,StaticLocalData.rows,GDK_RGB_DITHER_NONE,(guchar *)StaticLocalData.data,StaticLocalData.step[0]);
	gdk_threads_leave();

    return FALSE;
}

static gboolean icon_draw_expose_cb (GtkWidget *da, GdkEvent *event, gpointer _data)
{

    allData *data = (allData *)_data;


    return FALSE;
}

static void  *TEST(void * _data)
{
	if(fork() == 0)
		system("xdg-open /");
}

static void buttonFileManagerOpen_cb(GtkWidget *widget, allData *data){
	//shouldCapture = true;
	GError *error = NULL;

	char filename[200] = {0};
	sprintf(filename,"file:///",fileSaveLocation);
	pthread_t x;
	pthread_create(&x,NULL,&TEST,NULL);
	g_thread_new("footSwitchIcon", TEST,&data);


}




void gpio_thread_camera_reset(void *_data){
	allData *data = (allData *)_data;
	char direction_out[10] = {'o','u','t'};
	char direction_in[10] = {'i','n'};
	char value_0[1] = {'0'};
	char value_1[1] = {'1'};
	char read_buf[5] = {0};

	int fd = open( "/sys/class/gpio/gpio15/direction",O_WRONLY);
	if(write(fd,direction_out,3) != 3){
		g_printerr("Couldn't Write Direction\n");
	}
	close(fd);

	int fd_value = open( "/sys/class/gpio/gpio15/value",O_WRONLY);
	if(write(fd,value_0,1) != 1){
		g_printerr("Couldn't Write Direction\n");
	}
	close(fd_value);

	sleep(1);

		fd = open( "/sys/class/gpio/gpio15/value",O_WRONLY);
		if(write(fd,value_1,1) != 1){
			g_printerr("Couldn't Write Direction\n");
		}
		close(fd);


		sleep(1);
		fd = open( "/sys/class/gpio/gpio15/value",O_WRONLY);
		if(write(fd,value_0,1) != 1){
			g_printerr("Couldn't Write Direction\n");
		}
		close(fd);
		sleep(1);

}

void reset_acm(allData *data){
	pthread_t reset_acm_thread;
	//pthread_create(&reset_acm_thread,NULL, &gpio_thread_camera_reset,data);
}
