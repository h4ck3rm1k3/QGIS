/** \ingroup gui
 * A generic dialog with layout and button box
 */
class QgsDialog : QDialog
{
%TypeHeaderCode
#include <qgsdialog.h>
%End

  public:
    QgsDialog( QWidget *parent = 0, Qt::WindowFlags fl = QgisGui::ModalDialogFlags,
               QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Close,
               Qt::Orientation orientation = Qt::Horizontal );
    ~QgsDialog();

    //! Returns the central layout. Widgets added to it must have this dialog as parent.
    QVBoxLayout *layout();
    //! Returns the button box.
    QDialogButtonBox *buttonBox();
};
