/*
 *  SineGenerator.cpp - Simple sine wave generator
 *
 *  Written by Christian Bauer, Public Domain
 */

#include <AppKit.h>
#include <InterfaceKit.h>
#include <MediaKit.h>
#include <math.h>

#include "TSliderView.h"


// Sample frequency in Hz
const float SAMPLE_FREQ = 44100.0;

// Highest output frequency = SAMPLE_FREQ / Divisor
float Divisor = 4.0;


// Constants
const char APP_SIGNATURE[] = "application/x-vnd.cebix-SineGenerator";
const uint32 MSG_DIVISOR = 'divi';
const uint32 MSG_RESET_PHASE = 'rsph';
const rgb_color fill_color = {216, 216, 216, 0};


// Main window object
class MainWindow : public BWindow {
public:
	MainWindow();
	virtual bool QuitRequested(void);
	virtual void MessageReceived(BMessage *msg);

private:
	static void ampl_callback(float val, void *arg);
	static void freq_callback(float val, void *arg);
	static bool stream_func(void *arg, char *buf, size_t count, void *header);
	void calc_buffer(int16 *buf, size_t count);

	TSliderView *ampl_slider[4];
	TSliderView *freq_slider[4];

	BSubscriber *the_sub;
	BDACStream *the_stream;
	bool in_stream;

	float phi0, phi1, phi2, phi3;	// Phase angles
	bool reset_phase;				// Flag: reset phase angles
};


// Application object
class SineGenerator : public BApplication {
public:
	SineGenerator() : BApplication(APP_SIGNATURE) {}
	virtual void ReadyToRun(void) {new MainWindow;}
	virtual void AboutRequested(void);
};


/*
 *  Create application object and start it
 */

int main(int argc, char **argv)
{
	SineGenerator *the_app = new SineGenerator();
	the_app->Run();
	delete the_app;
	return 0;
}



/*
 *  About requested
 */

void SineGenerator::AboutRequested(void)
{
	BAlert *the_alert = new BAlert("", "Sine generator by Christian Bauer\n<cbauer@iphcip1.physik.uni-mainz.de>\nPublic domain.", "Neat");
	the_alert->Go();
}


/*
 *  Window creator
 */

MainWindow::MainWindow() : BWindow(BRect(0, 0, 599, 159), "SineGenerator", B_TITLED_WINDOW, B_NOT_RESIZABLE | B_NOT_ZOOMABLE)
{
	Lock();
	BRect b = Bounds();

	// Move window to right position
	MoveTo(80, 80);

	// Light gray background
	BView *top = new BView(BRect(0, 0, b.right, b.bottom), "", B_FOLLOW_NONE, B_WILL_DRAW);
	AddChild(top);
	top->SetViewColor(fill_color);

	// Labels
	BStringView *label = new BStringView(BRect(10, 10, 75, 20), "ampl_label", "Amplitude");
	top->AddChild(label);
	label = new BStringView(BRect(80, 10, 579, 20), "freq_label", "Frequency");
	top->AddChild(label);

	// Amplitude/frequency sliders
	for (int i=0; i<4; i++) {

		float ampl = i ? 0.0 : 0.5;
		float freq = 1000.0 / (SAMPLE_FREQ / Divisor);

		// Amplitude display
		BStringView *ampl_display = new BStringView(BRect(10, 38+i*30, 75, 48+i*30), "ampl_display", "");
		top->AddChild(ampl_display);
	
		// Amplitude slider
		ampl_slider[i] = new TSliderView(BRect(10, 20+i*30, 75, 37+i*30), "amplitude", ampl, ampl_callback, ampl_display);
		top->AddChild(ampl_slider[i]);
		ampl_callback(ampl, ampl_display);
	
		// Frequency display
		BStringView *freq_display = new BStringView(BRect(80, 38+i*30, 200, 48+i*30), "freq_display", "");
		top->AddChild(freq_display);
	
		// Frequency slider
		freq_slider[i] = new TSliderView(BRect(80, 20+i*30, 579, 37+i*30), "frequency", freq, freq_callback, freq_display);
		top->AddChild(freq_slider[i]);
		freq_callback(freq, freq_display);
	}
	reset_phase = true;

	// Divisor control
	BCheckBox *box = new BCheckBox(BRect(80, 140, 200, 155), "divisor", "/10", new BMessage(MSG_DIVISOR));
	top->AddChild(box);

	// Reset phase button
	BButton *button = new BButton(BRect(480, 130, 579, 155), "reset_phase", "Reset Phase", new BMessage(MSG_RESET_PHASE));
	top->AddChild(button);

	// Create and set up audio subscriber
	in_stream = false;
	the_sub = new BSubscriber("Sine Generator");
	the_stream = new BDACStream();
	if (the_sub->Subscribe(the_stream) == B_NO_ERROR) {
		the_sub->EnterStream(NULL, true, this, stream_func, NULL, true);
		the_stream->SetSamplingRate(SAMPLE_FREQ);
		in_stream = true;
	} else
		be_app->PostMessage(B_QUIT_REQUESTED);

	// Show window
	Show();
	Unlock();
}


/*
 *  Quit requested, close all and quit program
 */

bool MainWindow::QuitRequested(void)
{
	// Delete subscriber
	if (in_stream) {
		the_sub->ExitStream(true);
		in_stream = false;
	}
	the_sub->Unsubscribe();
	delete the_sub;
	delete the_stream;

	// Quit program
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}


/*
 *  Message received
 */

void MainWindow::MessageReceived(BMessage *msg)
{
	switch (msg->what) {
		case MSG_DIVISOR:
			if (Divisor != 4.0)
				Divisor = 4.0;
			else
				Divisor = 40.0;
			for (int i=0; i<4; i++)
				freq_slider[i]->SetValue(freq_slider[i]->Value());
			break;

		case MSG_RESET_PHASE:
			reset_phase = true;
			break;

		default:
			BWindow::MessageReceived(msg);
	}
}


/*
 *  Frequency slider moved, display frequency
 */

void MainWindow::freq_callback(float val, void *arg)
{
	char str[80];
	sprintf(str, "%d Hz", (int)(val * SAMPLE_FREQ / Divisor));
	((BStringView *)arg)->SetText(str);
}


/*
 *  Frequency slider moved, display frequency
 */

void MainWindow::ampl_callback(float val, void *arg)
{
	char str[80];
	sprintf(str, "%3.2f", val);
	((BStringView *)arg)->SetText(str);
}


/*
 *  Stream function 
 */

bool MainWindow::stream_func(void *arg, char *buf, size_t count, void *header)
{
	((MainWindow *)arg)->calc_buffer((int16 *)buf, count);
	return true;
}

void MainWindow::calc_buffer(int16 *buf, size_t count)
{
	float ampl0 = ampl_slider[0]->Value();
	float freq0 = freq_slider[0]->Value() * SAMPLE_FREQ / Divisor;
	float ampl1 = ampl_slider[1]->Value();
	float freq1 = freq_slider[1]->Value() * SAMPLE_FREQ / Divisor;
	float ampl2 = ampl_slider[2]->Value();
	float freq2 = freq_slider[2]->Value() * SAMPLE_FREQ / Divisor;
	float ampl3 = ampl_slider[3]->Value();
	float freq3 = freq_slider[3]->Value() * SAMPLE_FREQ / Divisor;

	if (reset_phase) {
		phi0 = phi1 = phi2 = phi3 = 0.0f;
		reset_phase = false;
	}

	count >>= 2;	// 16 bit stereo output, count is in bytes
	while (count--) {

		// Mix sinus waves
		float x = ampl0 * sin(phi0) + ampl1 * sin(phi1) + ampl2 * sin(phi2) + ampl3 * sin(phi3);

		// Convert elongation to int
		int data = (int)(x * 32767.0f);

		// Mix to left channel
		int val = *buf + data;
		if (val > 32767)
			val = 32767;
		if (val < -32768)
			val = -32768;
		*buf++ = val;

		// Mix to right channel
		val = *buf + data;
		if (val > 32767)
			val = 32767;
		if (val < -32768)
			val = -32768;
		*buf++ = val;

		// Update phase angles
		phi0 += 2.0f * M_PI * freq0 / SAMPLE_FREQ;
		while (phi0 > 2.0f * M_PI)
			phi0 -= 2.0f * M_PI;
		phi1 += 2.0f * M_PI * freq1 / SAMPLE_FREQ;
		while (phi1 > 2.0f * M_PI)
			phi1 -= 2.0f * M_PI;
		phi2 += 2.0f * M_PI * freq2 / SAMPLE_FREQ;
		while (phi2 > 2.0f * M_PI)
			phi2 -= 2.0f * M_PI;
		phi3 += 2.0f * M_PI * freq3 / SAMPLE_FREQ;
		while (phi3 > 2.0f * M_PI)
			phi3 -= 2.0f * M_PI;
	}
}
