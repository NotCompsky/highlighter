#include <QApplication>
#include <QDir>
#include "image-viewer.hpp"
#include <unistd.h> // for write

int main(int argc,  char *argv[]){
	if (argc < 2){
		constexpr const char* errmsg1 = "Error: No directory provided. Please specify the directory containing the images to be displayed.\n";
		write(2, errmsg1, std::char_traits<char>::length(errmsg1));
		return 1;
	}
	QApplication app(argc, argv);
	ImageViewer viewer(argv[1]);
	viewer.show();
	return app.exec();
}
