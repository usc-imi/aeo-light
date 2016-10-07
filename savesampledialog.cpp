#include "savesampledialog.h"
#include "ui_savesampledialog.h"

SaveSampleDialog::SaveSampleDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::SaveSampleDialog)
{
	ui->setupUi(this);
}

SaveSampleDialog::~SaveSampleDialog()
{
	delete ui;
}

int SaveSampleDialog::SelectedSlot() const
{
	if(ui->slot1RadioButton->isChecked()) return 1;
	else if(ui->slot2RadioButton->isChecked()) return 2;
	else if(ui->slot3RadioButton->isChecked()) return 3;
	else if(ui->slot4RadioButton->isChecked()) return 4;
	else return 0;
}
