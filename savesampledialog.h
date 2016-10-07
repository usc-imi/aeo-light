#ifndef SAVESAMPLEDIALOG_H
#define SAVESAMPLEDIALOG_H

#include <QDialog>

namespace Ui {
class SaveSampleDialog;
}

class SaveSampleDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SaveSampleDialog(QWidget *parent = 0);
	~SaveSampleDialog();
	int SelectedSlot() const;

private:
	Ui::SaveSampleDialog *ui;
};

#endif // SAVESAMPLEDIALOG_H
