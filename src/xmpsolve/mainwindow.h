#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QItemSelection>
#include "root.h"
#include "polynomialsolver.h"
#include "polfileeditordialog.h"
#include<mps/mps.h>

namespace Ui {
    class MainWindow;
}

namespace xmpsolve {

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
    void polynomial_solved();
    
private slots:
    void on_solveButton_clicked();
    void lockInterface();
    void unlockInterface();

    void on_openPolFileButton_clicked();

    void on_listRootsView_clicked(const QModelIndex &index);

    void on_editPolFileButton_clicked();

private:
    Ui::MainWindow *ui;
    PolynomialSolver m_solver;
    QString m_selectedPolFile;
};

} // Namespace xmpsolve

#endif // MAINWINDOW_H
