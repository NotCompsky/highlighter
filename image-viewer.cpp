#include "image-viewer.hpp"

#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QPaintEvent>
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMessageBox>
#include <QColorDialog>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QScrollArea>
#include <compsky/utils/ptrdiff.hpp>
#include <compsky/asciify/asciify.hpp>
#include <compsky/os/read.hpp>
#include <compsky/os/write.hpp>
#include <compsky/deasciify/a2f.hpp>
#include <openssl/sha.h>

constexpr const char* const fileid_to_boxes__filepath = "/home/vangelic/Documents/fileid_to_boxes.txt";
constexpr const char* const filepath_to_hsh__filepath = "/home/vangelic/Documents/filepath_to_hsh.txt";

const QColor DEFAULT_BOX_COLOUR = "#ffff00";

unsigned str_to_int(const std::string& s,  const std::pair<unsigned,unsigned>& offsets){
	unsigned n = 0;
	const char* const itr = s.data() + offsets.first;
	for (unsigned i = 0;  i < offsets.second;  ++i){
		const char c = itr[i];
		if (('0' > c) or (c > '9'))
			break;
		n *= 10;
		n += c - '0';
	}
	if (n == 0)
		n = -1;
	return n;
}

ImageViewer::ImageViewer(const char* const srcDir,  QWidget* parent)
: QWidget(parent)
, imgdir(srcDir)
, currentIndex_(-1)
, zoomLevel_(1.0)
, image_(new QImage())
, boxes_()
, isDragging_(false)
, found_local_metadata_file(false)
{
	fileid_to_boxes__filepath__local = reinterpret_cast<char*>(malloc(strlen(srcDir+20)));
	{
		char* _itr1 = fileid_to_boxes__filepath__local;
		compsky::asciify::asciify(_itr1, srcDir, '/', "_filename_to_boxes.txt", '\0');
	}
	
	{
		DIR* const dir = opendir(srcDir);
		if (unlikely(dir == nullptr)){
			QMessageBox::warning(this, "Error", "The provided directory doesn't exist.");
		} else {
			struct dirent* ent;
			while((ent = readdir(dir))){
				if (likely((ent->d_type == DT_REG) or (ent->d_type == DT_LNK))){
					const std::size_t fsz = strlen(ent->d_name);
					if (fsz == 22){
						if (
							(ent->d_name[0]  == '_') and
							(ent->d_name[1]  == 'f') and
							(ent->d_name[2]  == 'i') and
							(ent->d_name[3]  == 'l') and
							(ent->d_name[4]  == 'e') and
							(ent->d_name[5]  == 'n') and
							(ent->d_name[6]  == 'a') and
							(ent->d_name[7]  == 'm') and
							(ent->d_name[8]  == 'e') and
							(ent->d_name[9]  == '_') and
							(ent->d_name[10] == 't') and
							(ent->d_name[11] == 'o') and
							(ent->d_name[12] == '_') and
							(ent->d_name[13] == 'b') and
							(ent->d_name[14] == 'o') and
							(ent->d_name[15] == 'x') and
							(ent->d_name[16] == 'e') and
							(ent->d_name[17] == 's') and
							(ent->d_name[18] == '.') and
							(ent->d_name[19] == 't') and
							(ent->d_name[20] == 'x') and
							(ent->d_name[21] == 't')
						){
							this->found_local_metadata_file = true;
							continue;
						}
					}
					if (unlikely(
						(fsz < 5) or
						(
							(
								(ent->d_name[fsz-1] != 'g') or
								(ent->d_name[fsz-2] != 'p') or
								(ent->d_name[fsz-3] != 'j') or
								(ent->d_name[fsz-4] != '.')
							) and (
								(ent->d_name[fsz-1] != 'g') or
								(ent->d_name[fsz-2] != 'n') or
								(ent->d_name[fsz-3] != 'p') or
								(ent->d_name[fsz-4] != '.')
							)
						)
					)){
						printf("Not a .jpg or .png file: %.*s\n", (int)fsz, ent->d_name); // NOTE: Don't really need to restrict to PNG
					}
					filename_offsets.emplace_back(filenames.size(), fsz);
					filenames += ent->d_name;
				}
			}
			if (unlikely(filenames.size() == 0)){
				QMessageBox::warning(this, "Error", "The provided directory is empty.");
			} else {
				std::sort(filename_offsets.begin(), filename_offsets.end(), [this](const std::pair<unsigned,unsigned>& offsets1,  const std::pair<unsigned,unsigned>& offsets2){
					return (str_to_int(filenames, offsets1) < str_to_int(filenames, offsets2));
				});
				
				//setWindowSize(QSize(960, 1080));
				memcpy(this->filepath, srcDir, strlen(srcDir));
				this->filepath[strlen(srcDir)] = '/';
				this->relative_filepath = this->filepath + strlen(srcDir) + 1;
				printf("found_local_metadata_file: %u\n", (unsigned)found_local_metadata_file);
				this->loadAllBoxes();
				if (found_local_metadata_file){
					this->filepaths2hashes.reserve(this->filename_offsets.size());
					this->file_ids.resize(this->filename_offsets.size());
					for (unsigned i = 0;  i < this->filename_offsets.size();  ++i){
						const std::pair<unsigned,unsigned>& file_offset = this->filename_offsets[i];
						const std::string_view filename(this->filenames.data()+file_offset.first, file_offset.second);
						printf("filename %.*s\n", (int)filename.size(), filename.data());
						this->filepaths2hashes.emplace_back(filename);
						memset(file_ids[i].as_str, 0, 32);
						memcpy(file_ids[i].as_str, filename.data(), filename.size());
					}
				} else {
					this->load_file_ids();
				}
				/*QVBoxLayout* overview_layout = new QVBoxLayout(this);
				QWidget *checkBoxesWidget = new QWidget(this);
				QVBoxLayout* checkBoxesLayout = new QVBoxLayout(checkBoxesWidget);
				for (const QString &file : fileList_) {
					QCheckBox *checkBox = new QCheckBox(file, checkBoxesWidget);
					checkBoxesLayout->addWidget(checkBox);
				}
				this->overview_widget = reinterpret_cast<QWidget*>(new QScrollArea(this));
				/*reinterpret_cast<QScrollArea*>(this->overview_widget)->setWidget(checkBoxesWidget);
				overview_layout->addWidget(this->overview_widget);*/
				
				this->display_overview_page();
			}
		}
	}
}

void ImageViewer::display_overview_page(){
	this->is_displaying_img_ = false;
	//this->overview_widget->setVisible(true);
	char buf[100];
	this->count_files_with_boxes(buf);
	setWindowTitle(buf);
}

bool hashes_different(const char* hash1,  const char(&hash2)[32]){
	const uint64_t* ints1 = reinterpret_cast<const uint64_t*>(hash1);
	const uint64_t* ints2 = reinterpret_cast<const uint64_t*>(hash2);
	return (ints1[0] ^ ints2[0]) | (ints1[1] ^ ints2[1]) | (ints1[2] ^ ints2[2]) | (ints1[3] ^ ints2[3]);
}
bool hashes_different(const char* hash1,  const unsigned char(&hash2)[32]){
	return hashes_different(hash1, reinterpret_cast<const char(&)[32]>(hash2));
}

void ImageViewer::sort_out_boxes_sdffsfsdf(char(&full_file_path)[4096],  std::vector<char*>& hash_ptrs){
	char* const last_hash_ptr = hash_ptrs[hash_ptrs.size()-1];
	for (Box& box : this->boxes_){
		for (char* hash : hash_ptrs){
			if (unlikely(not hashes_different(hash, box.file_id.as_str))){
				memcpy(box.file_id.as_str, last_hash_ptr, 32);
			}
		}
	}
}

void ImageViewer::load_file_ids(){
	// TODO: Handle transitive file path moves (e.g. filepath1 -> hash1, filepath1 -> hash2, filepath2 -> hash2;  filepath1 -> hash1, filepath2 -> hash1, filepath2 -> hash2)
	file_ids.resize(filename_offsets.size());
	unsigned char sha2[SHA256_DIGEST_LENGTH];
	for (unsigned i = 0;  i < filename_offsets.size();  ++i){
		memset(file_ids[i].as_str, 0, 32);
		this->set_img_path(i);
		compsky::os::ReadOnlyFile f(this->filepath);
		if (likely(not f.is_null())){
			char full_file_path[4096];
			std::vector<char*> hash_ptrs;
			char* hash_ptr = nullptr;
			if (likely(realpath(this->relative_filepath, full_file_path) != nullptr)){
				for (Filepath2Hash& fp2hash : this->filepaths2hashes){
					if (fp2hash.same_path(full_file_path)){
						hash_ptrs.emplace_back(fp2hash.hash.as_str);
					}
				}
				if (hash_ptrs.size() != 0){
					if (unlikely(hash_ptrs.size() > 1)){
						this->sort_out_boxes_sdffsfsdf(full_file_path, hash_ptrs);
					}
					hash_ptr = hash_ptrs[hash_ptrs.size()-1];
					memcpy(file_ids[i].as_str, hash_ptr, 32);
					// Should calculate hash again anyway - because file might have been edited  continue;
				}
			}
			
			const std::size_t f_sz = f.size();
			char* const buffer = reinterpret_cast<char*>(malloc(f_sz));
			if (likely(buffer != nullptr)){
				f.read_into_buf(buffer, f_sz);
				
				SHA256_CTX ctx;
				SHA256_Init(&ctx);
				SHA256_Update(&ctx, buffer, f_sz);
				SHA256_Final(sha2, &ctx);
				if ((hash_ptr == nullptr) or (hashes_different(hash_ptr, sha2))){
					this->filepaths2hashes.emplace_back(this->filepath, sha2);
				}
				memcpy(
					file_ids[i].as_str,
					(hash_ptr == nullptr) ? reinterpret_cast<char*>(&sha2[0]) : hash_ptr,
					32
				);
			}
			free(buffer);
		}
	}
}

ImageViewer::~ImageViewer(){
	delete image_;
}

void ImageViewer::set_img_path(const int index){
	const std::pair<unsigned,unsigned>& filename_offset = filename_offsets[index];
	const std::string_view file_name(filenames.data() + filename_offset.first, filename_offset.second);
	memcpy(this->relative_filepath, file_name.data(), file_name.size());
	this->relative_filepath[file_name.size()] = 0;
}

void ImageViewer::loadImage(int index){
	if (index < 0){
		index = -1;
		currentIndex_ = index;
		this->display_overview_page();
	}
	if ((index == -1) or (index >= filename_offsets.size())){
		return;
	}
	//this->overview_widget->setVisible(false);
	currentIndex_ = index;
	
	this->set_img_path(currentIndex_);
	
	const std::pair<unsigned,unsigned>& filename_offset = filename_offsets[index];
	const std::string_view file_name(filenames.data() + filename_offset.first, filename_offset.second);
	setWindowTitle(QString::fromLocal8Bit(file_name.data(), file_name.size()));
	
	QImageReader because_this_reads_exif(this->filepath);
	because_this_reads_exif.setAutoTransform(true);
	if (likely(because_this_reads_exif.read(image_))){
		this->is_displaying_img_ = true;
		this->update_scale();
		update();
	} else {
		this->is_displaying_img_ = false;
		QMessageBox::warning(this, "Error", QString("Failed to load image: %1").arg(this->filepath));
	}
}

void ImageViewer::loadAllBoxes(){
	boxes_.clear();
	char* buffer;
	std::size_t f_sz;
	if (this->found_local_metadata_file){
		compsky::os::ReadOnlyFile f(this->fileid_to_boxes__filepath__local);
		if (unlikely(f.is_null()))
			return;
		
		f_sz = f.size();
		buffer = reinterpret_cast<char*>(malloc(f_sz));
		f.read_into_buf(buffer, f_sz);
	} else { // Old
		compsky::os::ReadOnlyFile f(fileid_to_boxes__filepath);
		if (unlikely(f.is_null()))
			return;
		
		compsky::os::ReadOnlyFile ff(filepath_to_hsh__filepath);
		if (ff.size<0>() != 0){
			const size_t ff_sz = ff.size<0>();
			this->filepaths2hashes.resize(ff_sz / sizeof(Filepath2Hash));
			ff.read_into_buf(reinterpret_cast<char*>(this->filepaths2hashes.data()), ff_sz);
		}
		
		f_sz = f.size();
		buffer = reinterpret_cast<char*>(malloc(f_sz));
		f.read_into_buf(buffer, f_sz);
	}
	char* lineStart = buffer;
	for (int i = 0;  i < f_sz;  ++i){
		if (buffer[i] == '\n') {
			parseBoxLine(lineStart, this->found_local_metadata_file);
			lineStart = buffer + i + 1;
		}
	}
	if (lineStart < buffer + f_sz){
		parseBoxLine(lineStart, this->found_local_metadata_file);
	}
	free(buffer);
}

void ImageViewer::parseBoxLine(const char* line,  const bool is_from_local_metadata_file){
	Box box;
	memset(box.file_id.as_str, 0, 32);
	if (is_from_local_metadata_file){
		const char* const filename_start = line;
		while (*line != '/')
			++line;
		const std::size_t filename_sz = compsky::utils::ptrdiff(line,filename_start);
		if (unlikely(filename_sz > 32)){
			printf("Filename too large: %.*s\n", (int)filename_sz, filename_start); fflush(stdout);
			abort();
		} else {
			memcpy(box.file_id.as_str, filename_start, filename_sz);
		}
	}
	box.x = a2f<float,const char**,true>(&line);
	if (likely(line[0] == ',')){
		++line;
		box.y = a2f<float,const char**,true>(&line);
		if (likely(line[0] == ',')){
			++line;
			box.w = a2f<float,const char**,true>(&line);
			if (likely(line[0] == ',')){
				++line;
				box.h = a2f<float,const char**,true>(&line);
				if (likely(line[0] == ',')){
					++line;
					if (likely(line[7] == ',')){
						memcpy(box.colour, line, 7);
						box.colour[7] = 0;
						line += 7;
						if (not is_from_local_metadata_file){
							++line;
							memcpy(box.file_id.as_str, line, 32);
						}
						boxes_.append(box);
					}
				}
			}
		}
	}
}

void ImageViewer::saveBoxes(){
	compsky::os::WriteOnlyFile f1(fileid_to_boxes__filepath);
	compsky::os::WriteOnlyFile f2(this->fileid_to_boxes__filepath__local);
	const std::size_t buf_sz = boxes_.size()*(4*10+1 + 6+1 + 32+1);
	char* const buffer = reinterpret_cast<char*>(malloc(buf_sz));
	  // fileHash.constData()
	{ // Write ALL hashes to the global file
		char* itr = buffer;
		for (const Box &box : boxes_){
			compsky::asciify::asciify(itr, box.x,6,',', box.y,6,',', box.w,6,',', box.h,6,',', box.colour, ',', std::string_view(box.file_id.as_str,32), '\n');
		}
		f1.write_from_buffer(buffer, compsky::utils::ptrdiff(itr,buffer));
	}
	{ // Write only LOCAL hashes to the local file
		char* itr = buffer;
		for (const Box &box : boxes_){
			unsigned whichfile = -1;
			for (unsigned i = 0;  i < file_ids.size();  ++i){
				const Hash& current_file_id = file_ids[i];
				if (
					(box.file_id.as_int[0] != current_file_id.as_int[0]) or
					(box.file_id.as_int[1] != current_file_id.as_int[1]) or
					(box.file_id.as_int[2] != current_file_id.as_int[2]) or
					(box.file_id.as_int[3] != current_file_id.as_int[3])
				)
					continue;
				whichfile = i;
			}
			if (whichfile != -1){
				const std::pair<unsigned,unsigned>& filename_offset = filename_offsets[whichfile];
				const std::string_view file_name(filenames.data() + filename_offset.first, filename_offset.second);
				
				compsky::asciify::asciify(itr, file_name, '/', box.x,6,',', box.y,6,',', box.w,6,',', box.h,6,',', box.colour, '\n');
			}
		}
		f2.write_from_buffer(buffer, compsky::utils::ptrdiff(itr,buffer));
	}
	
	compsky::os::WriteOnlyFile ff(filepath_to_hsh__filepath);
	ff.write_from_buffer(reinterpret_cast<char*>(this->filepaths2hashes.data()), this->filepaths2hashes.size()*sizeof(Filepath2Hash));
}

unsigned ImageViewer::count_files_with_boxes(char* buf) const {
	unsigned file_count = 0;
	unsigned box_count = 0;
	for (unsigned i = 0;  i < file_ids.size();  ++i){
		const Hash& current_file_id = file_ids[i];
		bool is_matched = false;
		for (const Box& box : boxes_){
			if (
				(box.file_id.as_int[0] != current_file_id.as_int[0]) or
				(box.file_id.as_int[1] != current_file_id.as_int[1]) or
				(box.file_id.as_int[2] != current_file_id.as_int[2]) or
				(box.file_id.as_int[3] != current_file_id.as_int[3])
			)
				continue;
			is_matched = true;
			++box_count;
		}
		file_count += is_matched;
	}
	compsky::asciify::asciify(buf, file_count, " files have ", box_count, " boxes", '\0');
	return file_count;
}

void ImageViewer::paintEvent(QPaintEvent *event){
	if (not this->is_displaying_img_)
		return;
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	
	QRect imageRect(
		img_scaled_offset_x_,
		img_scaled_offset_y_,
		img_scaled_w_,
		img_scaled_h_
	);
	
	painter.drawImage(imageRect, *image_, image_->rect());
	for (Box& box : boxes_){
		fflush(stdout);
			
		const unsigned matches = (
			(box.file_id.as_int[0] == file_ids[currentIndex_].as_int[0]) +
			(box.file_id.as_int[1] == file_ids[currentIndex_].as_int[1]) +
			(box.file_id.as_int[2] == file_ids[currentIndex_].as_int[2]) +
			(box.file_id.as_int[3] == file_ids[currentIndex_].as_int[3])
		);
		if (matches == 0)
			continue;
		if (matches != 4){
			printf(
				"NEAR HIT, %u matches:\n\t%lu\t%lu\t%lu\t%lu\tvs\t%lu\t%lu\t%lu\t%lu\n",
				matches,
				box.file_id.as_int[0], box.file_id.as_int[1], box.file_id.as_int[2], box.file_id.as_int[3],
				file_ids[currentIndex_].as_int[0], file_ids[currentIndex_].as_int[1], file_ids[currentIndex_].as_int[2], file_ids[currentIndex_].as_int[3]
			);
			memcpy(box.file_id.as_str, file_ids[currentIndex_].as_str, 32);
		}
		if ((box.colour[0]=='#')and(box.colour[1]=='0')and(box.colour[2]=='0')and(box.colour[3]=='0')and(box.colour[4]=='0')and(box.colour[5]=='0')and(box.colour[6]=='0')){
			painter.setOpacity(1.0);
			painter.setClipRegion(imageRectForBox(box));
			painter.drawImage(imageRect, *image_, image_->rect());
			painter.setClipping(false);
		} else {
			painter.setOpacity(0.5);
			painter.fillRect(imageRectForBox(box), QColor(box.colour));
		}
	}
	if (isDragging_){
		painter.setOpacity(0.5);
		QRect rect1 = dragRect();
		QRect rect2(
			rect1.x() + img_scaled_offset_x_,
			rect1.y() + img_scaled_offset_y_,
			rect1.width(),
			rect1.height()
		);
		painter.fillRect(rect2, DEFAULT_BOX_COLOUR);
	}
}

void ImageViewer::keyPressEvent(QKeyEvent *event){
	if (event->key() == Qt::Key_Left) {
		loadImage(currentIndex_ - 1);
	} else if (event->key() == Qt::Key_Right) {
		loadImage(currentIndex_ + 1);
	}
}

void ImageViewer::mousePressEvent(QMouseEvent *event){
	if (not this->is_displaying_img_)
		return;
	if (event->button() == Qt::LeftButton){
		const QPoint widgetPos = this->pos();
		const QPoint dragStart = event->pos();
		this->dragStart_x = dragStart.x() - img_scaled_offset_x_;
		this->dragStart_y = dragStart.y() - img_scaled_offset_y_;
		isDragging_ = true;
	}
}

void ImageViewer::mouseMoveEvent(QMouseEvent *event){
	if (not this->is_displaying_img_)
		return;
	if (isDragging_){
		update();
	}
}

void ImageViewer::mouseReleaseEvent(QMouseEvent *event){
	if (not this->is_displaying_img_)
		return;
	if (event->button() == Qt::LeftButton) {
		isDragging_ = false;
		QRect rect = dragRect();
		if ((rect.width() > 0) and (rect.height() > 0)){
			const QColor boxColour = selectBoxColour();
			if (boxColour.isValid()){
				Box box;
				box.x = rect.x() / img_scaled_w_;
				box.y = rect.y() / img_scaled_h_;
				box.w = rect.width() / img_scaled_w_;
				box.h = rect.height() / img_scaled_h_;
				memcpy(box.colour, boxColour.name().toLatin1().constData(), 7);
				memcpy(box.file_id.as_str, file_ids[currentIndex_].as_str, 32);
				box.colour[7] = 0;
				boxes_.append(box);
				saveBoxes();
			}
			update();
		}
	}
}

QColor ImageViewer::selectBoxColour() const {
	QColor colour = QColorDialog::getColor();
	return colour;
}

void ImageViewer::wheelEvent(QWheelEvent *event){
	if (not this->is_displaying_img_)
		return;
	
	const QPoint p = QCursor::pos();
	const QPoint q = this->pos();
	const float x = p.x() - q.x();
	const float y = p.y() - q.y();
	printf("COORDS %f %f\n", x, y);
	
	int numDegrees = event->delta() / 8;
	int numSteps = numDegrees / 15;
	zoomLevel_ += numSteps * 0.1;
	if (zoomLevel_ < 1.0){
		zoomLevel_ = 1.0;
	}
	this->update_scale();
	update();
}

void ImageViewer::update_scale(){
	if (not this->is_displaying_img_)
		return;
	double scaleX = width() / (double)image_->width();
	double scaleY = height() / (double)image_->height();
	scale_ = qMin(scaleX, scaleY);
	img_scaled_w_ = image_->width() * scale_ * zoomLevel_;
	img_scaled_h_ = image_->height() * scale_ * zoomLevel_;
	img_scaled_offset_x_ = (width()  - img_scaled_w_) / 2;
	img_scaled_offset_y_ = (height() - img_scaled_h_) / 2;
	img_offset_x_ = ( width()/(scale_*zoomLevel_) -  image_->width())/2.0;
	img_offset_y_ = (height()/(scale_*zoomLevel_) - image_->height())/2.0;
	printf("SCALE %f %f\n",  img_offset_x_ * scale_ * zoomLevel_,  img_offset_y_ * scale_ * zoomLevel_);
}

void ImageViewer::resizeEvent(QResizeEvent *event){
	QWidget::resizeEvent(event);
	this->update_scale();
}

QRect ImageViewer::imageRectForBox(const Box &box) const {
	int x = img_scaled_w_ * box.x + img_scaled_offset_x_;
	int y = img_scaled_h_ * box.y + img_scaled_offset_y_;
	int w = img_scaled_w_ * box.w;
	int h = img_scaled_h_ * box.h;
	return QRect(x, y, w, h);
}

QRect ImageViewer::dragRect() const {
	const QPoint p = QCursor::pos();
	const QPoint q = this->pos();
	int x2 = p.x() - q.x() - img_scaled_offset_x_;
	int y2 = p.y() - q.y() - img_scaled_offset_y_;
	return QRect(qMin(dragStart_x, x2), qMin(dragStart_y, y2), qAbs(dragStart_x - x2), qAbs(dragStart_y - y2));
}
