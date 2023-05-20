#ifndef IMAGEVIEWER_HPP
#define IMAGEVIEWER_HPP

#include <QWidget>
#include <QColor>
#include <vector>
#include <compsky/macros/likely.hpp>

class QImage;

struct AsIndex {
	uint64_t a;
	uint64_t b;
	uint64_t indx;
	uint64_t d;
};

struct Hash {
	union {
		char as_str[32];
		uint64_t as_int[4];
		AsIndex as_indx;
	};
};

class ImageViewer : public QWidget {
	Q_OBJECT
 public:
	explicit ImageViewer(const char* const srcDir,  QWidget* parent = nullptr);
	~ImageViewer();
 private:
	struct Filepath2Hash {
		char filepath[4096];
		Hash hash;
		Filepath2Hash(const std::string_view& _filepath_as_hash){
			if (unlikely(_filepath_as_hash.size() > 32)){
				printf("Filename too large: %.*s\n", (int)_filepath_as_hash.size(), _filepath_as_hash.data()); fflush(stdout);
				abort();
			}
			memset(this->filepath, 0, 4096);
			memcpy(this->filepath, _filepath_as_hash.data(), _filepath_as_hash.size());
			memset(this->hash.as_str, 0, 32);
			memcpy(this->hash.as_str, _filepath_as_hash.data(), _filepath_as_hash.size());
		}
		Filepath2Hash(char(&_filepath)[4096],  unsigned char(&_hash)[32]){
			memset(this->filepath, 0, 4096);
			memcpy(this->filepath, _filepath, strlen(_filepath));
			memcpy(this->hash.as_str, reinterpret_cast<char*>(_hash), 32);
		}
		Filepath2Hash(){}
		Filepath2Hash(const Filepath2Hash& that){
			memcpy(this->filepath, that.filepath, 4096);
			memcpy(this->hash.as_str, that.hash.as_str, 32);
		}
		~Filepath2Hash(){}
		bool same_path(char(&_filepath)[4096]) const {
			bool rc = true;
			unsigned i;
			for (i = 0;  _filepath[i] == filepath[i];  ++i);
			return (filepath[i-1] == 0); // NOTE: All paths guaranteed to begin with a / so i>=1
		};
	};
	std::vector<Filepath2Hash> filepaths2hashes;
	bool filepath_recorded;
	
	void loadImage(int index);
	void loadAllBoxes();
	void parseBoxLine(const char* line,  const bool is_from_local_metadata_file);
	void saveBoxes();
	
	struct Box {
		float x;
		float y;
		float w;
		float h;
		char colour[8];
		union {
			char as_str[32];
			uint64_t as_int[4];
		} file_id;
	};
	
	QRect imageRectForZoom() const;
	QRect imageRectForBox(const Box &box) const;
	QRect dragRect() const;
	QColor selectBoxColour() const;
	
	void paintEvent(QPaintEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	
	void sort_out_boxes_sdffsfsdf(char(&)[4096],  std::vector<char*>&);
	void load_file_ids();
	unsigned count_files_with_boxes(char* buf) const;
	void update_scale();
	void set_img_path(const int);
	void display_overview_page();
	
	char* fileid_to_boxes__filepath__local;
	
	char filepath[4096];
	char* relative_filepath;
	const char* const imgdir;
	std::string filenames;
	std::vector<std::pair<unsigned,unsigned>> filename_offsets;
	int currentIndex_;
	double zoomLevel_;
	double scale_;
	float img_scaled_w_;
	float img_scaled_h_;
	float img_offset_x_;
	float img_offset_y_;
	QWidget* overview_widget;
	QImage* image_;
	QList<Box> boxes_;
	int dragStart_x;
	int dragStart_y;
	int img_scaled_offset_x_;
	int img_scaled_offset_y_;
	bool isDragging_;
	bool is_displaying_img_;
	bool found_local_metadata_file;
	std::vector<Hash> file_ids;
};

#endif // IMAGEVIEWER_HPP
