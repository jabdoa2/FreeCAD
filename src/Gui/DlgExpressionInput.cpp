/***************************************************************************
 *   Copyright (c) 2015 Eivind Kvedalen <eivind@kvedalen.name>             *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 51 Franklin Street,      *
 *   Fifth Floor, Boston, MA  02110-1301, USA                              *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"
#include <QApplication>
#include <QPainter>
#include <QDesktopWidget>
#include <QMouseEvent>

#include "DlgExpressionInput.h"
#include "ui_DlgExpressionInput.h"
#include "ExpressionCompleter.h"
#include <Base/Tools.h>
#include <Base/Console.h>
#include <App/Expression.h>
#include <App/DocumentObject.h>

using namespace App;
using namespace Gui::Dialog;

DlgExpressionInput::DlgExpressionInput(const App::ObjectIdentifier & _path,
                                       boost::shared_ptr<const Expression> _expression,
                                       const Base::Unit & _impliedUnit, QWidget *parent)
  : QDialog(parent)
  , ui(new Ui::DlgExpressionInput)
  , expression(_expression ? _expression->copy() : 0)
  , path(_path)
  , discarded(false)
  , impliedUnit(_impliedUnit)
  , minimumWidth(10)
{
    assert(path.getDocumentObject() != 0);

    // Setup UI
    ui->setupUi(this);

    if (expression) {
        ui->expression->setText(Base::Tools::fromStdString(expression->toString()));
        textChanged(Base::Tools::fromStdString(expression->toString()));
    }

    // Connect signal(s)
    connect(ui->expression, SIGNAL(textChanged(QString)), this, SLOT(textChanged(QString)));
    connect(ui->discardBtn, SIGNAL(clicked()), this, SLOT(setDiscarded()));

    // Set document object on line edit to create auto completer
    DocumentObject * docObj = path.getDocumentObject();
    ui->expression->setDocumentObject(docObj);

    setWindowFlags(Qt::SubWindow | Qt::Widget | Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);

    ui->expression->setFocus();
    qApp->installEventFilter(this);
}

DlgExpressionInput::~DlgExpressionInput()
{
    qApp->removeEventFilter(this);
    delete ui;
}

QPoint DlgExpressionInput::expressionPosition() const
{
    return ui->expression->pos();
}

void DlgExpressionInput::textChanged(const QString &text)
{
    try {
        //resize the input field according to text size
        QFontMetrics fm(ui->expression->font());
        int width = fm.width(text) + 15;
        if (width < minimumWidth)
            ui->expression->setMinimumWidth(minimumWidth);
        else
            ui->expression->setMinimumWidth(width);
        
        if(this->width() < ui->expression->minimumWidth())
            setMinimumWidth(ui->expression->minimumWidth());

        //now handle expression
        boost::shared_ptr<Expression> expr(ExpressionParser::parse(path.getDocumentObject(), text.toUtf8().constData()));

        if (expr) {
            std::string error = path.getDocumentObject()->ExpressionEngine.validateExpression(path, expr);

            if (error.size() > 0)
                throw Base::Exception(error.c_str());

            std::auto_ptr<Expression> result(expr->eval());

            expression = expr;
            ui->okBtn->setEnabled(true);
            ui->msg->clear();

            NumberExpression * n = Base::freecad_dynamic_cast<NumberExpression>(result.get());
            if (n) {
                Base::Quantity value = n->getQuantity();

                if (!value.getUnit().isEmpty() && value.getUnit() != impliedUnit)
                    throw Base::Exception("Unit mismatch between result and required unit");

                value.setUnit(impliedUnit);

                ui->msg->setText(value.getUserString());
            }
            else
                ui->msg->setText(Base::Tools::fromStdString(result->toString()));

            //set default palette as we may have read text right now
            ui->msg->setPalette(ui->okBtn->palette());
        }
    }
    catch (Base::Exception & e) {
        ui->msg->setText(QString::fromUtf8(e.what()));
        QPalette p(ui->msg->palette());
        p.setColor(QPalette::WindowText, Qt::red);
        ui->msg->setPalette(p);
        ui->okBtn->setDisabled(true);
    }
}

void DlgExpressionInput::setDiscarded()
{
    discarded = true;
    reject();
}

void DlgExpressionInput::setExpressionInputSize(int width, int height)
{
    if (ui->expression->minimumHeight() < height)
        ui->expression->setMinimumHeight(height);

    if (ui->expression->minimumWidth() < width)
        ui->expression->setMinimumWidth(width);

    minimumWidth = width;
}

void DlgExpressionInput::mouseReleaseEvent(QMouseEvent* ev)
{
#if 0//defined(Q_WS_WIN)
    if (QWidget::mouseGrabber() == this) {
        QList<QWidget*> childs = this->findChildren<QWidget*>();
        for (QList<QWidget*>::iterator it = childs.begin(); it != childs.end(); ++it) {
            QPoint pos = (*it)->mapFromGlobal(ev->globalPos());
            if ((*it)->rect().contains(pos)) {
                // Create new mouse event with the correct local position
                QMouseEvent me(ev->type(), pos, ev->globalPos(), ev->button(), ev->buttons(), ev->modifiers());
                QObject* obj = *it;
                obj->event(&me);
                if (me.isAccepted()) {
                    break;
                }
            }
        }
    }
#else
    Q_UNUSED(ev);
#endif
}

void DlgExpressionInput::mousePressEvent(QMouseEvent* ev)
{
#if 0//defined(Q_WS_WIN)
    bool handled = false;
    if (QWidget::mouseGrabber() == this) {
        QList<QWidget*> childs = this->findChildren<QWidget*>();
        for (QList<QWidget*>::iterator it = childs.begin(); it != childs.end(); ++it) {
            QPoint pos = (*it)->mapFromGlobal(ev->globalPos());
            if ((*it)->rect().contains(pos)) {
                // Create new mouse event with the correct local position
                QMouseEvent me(ev->type(), pos, ev->globalPos(), ev->button(), ev->buttons(), ev->modifiers());
                QObject* obj = *it;
                obj->event(&me);
                if (me.isAccepted()) {
                    handled = true;
                    break;
                }
            }
        }
    }

    if (handled)
        return;
#else
    Q_UNUSED(ev);
#endif
    //we need to reject the dialog when clicked on the background. As the background is transparent
    //this is the expected behaviour for the user
    this->reject();
}

void DlgExpressionInput::showEvent(QShowEvent* ev)
{
    QDialog::showEvent(ev);

#if 0//defined(Q_WS_WIN)
    // This way we can fetch click events outside modal dialogs
    QWidget* widget = QApplication::activeModalWidget();
    if (widget) {
        QList<QWidget*> childs = widget->findChildren<QWidget*>();
        if (childs.contains(this)) {
            this->grabMouse();
        }
    }
#endif
}

bool DlgExpressionInput::eventFilter(QObject *obj, QEvent *ev)
{
    // if the user clicks on a widget different to this
    if (ev->type() == QEvent::MouseButtonPress && obj != this) {
        // Since the widget has a transparent background we cannot rely
        // on the size of the widget. Instead, it must be checked if the
        // cursor is on this or an underlying widget or outside.
        if (!underMouse()) {
            qApp->removeEventFilter(this);
            reject();
        }
    }

    return false;
}


#include "moc_DlgExpressionInput.cpp"
