/***************************************************************************
 *   Copyright (c) 2009 Juergen Riegel <juergen.riegel@web.de>             *
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
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"

#ifndef _PreComp_
# include <cmath>
# include <QContextMenuEvent>
# include <QMenu>
# include <QRegExp>
# include <QString>
# include <QMessageBox>
# include <QStyledItemDelegate>
# include <QPainter>
# include <QPixmapCache>
#endif

#include "TaskSketcherConstrains.h"
#include "ui_TaskSketcherConstrains.h"
#include "EditDatumDialog.h"
#include "ViewProviderSketch.h"

#include <Mod/Sketcher/App/SketchObject.h>

#include <Base/Tools.h>
#include <App/Application.h>
#include <App/Document.h>
#include <Gui/Application.h>
#include <Gui/Document.h>
#include <Gui/Selection.h>
#include <Gui/BitmapFactory.h>
#include <Gui/ViewProvider.h>
#include <boost/bind.hpp>
#include <Gui/Command.h>
#include <Gui/MainWindow.h>

using namespace SketcherGui;
using namespace Gui::TaskView;

/// Inserts a QAction into an existing menu
/// ICONSTR is the string of the icon in the resource file
/// NAMESTR is the text appearing in the contextual menuAction
/// CMDSTR is the string registered in the commandManager
/// FUNC is the name of the member function to be executed on selection of the menu item
/// ACTSONSELECTION is a true/false value to activate the command only if a selection is made
#define CONTEXT_ITEM(ICONSTR,NAMESTR,CMDSTR,FUNC,ACTSONSELECTION)                       \
QIcon icon_ ## FUNC( Gui::BitmapFactory().pixmap(ICONSTR) );                    \
    QAction* constr_ ## FUNC = menu.addAction(icon_ ## FUNC,tr(NAMESTR), this, SLOT(FUNC()),    \
        QKeySequence(QString::fromUtf8(Gui::Application::Instance->commandManager().getCommandByName(CMDSTR)->getAccel())));        \
    if(ACTSONSELECTION) constr_ ## FUNC->setEnabled(!items.isEmpty()); else constr_ ## FUNC->setEnabled(true);
/// Defines the member function corresponding to the CONTEXT_ITEM macro
#define CONTEXT_MEMBER_DEF(CMDSTR,FUNC)                             \
void ConstraintView::FUNC(){                               \
   Gui::Application::Instance->commandManager().runCommandByName(CMDSTR);}

// helper class to store additional information about the listWidget entry.
class ConstraintItem : public QListWidgetItem
{
public:
    ConstraintItem(const Sketcher::SketchObject * s, int ConstNbr)
        : QListWidgetItem(QString()),
          sketch(s),
          ConstraintNbr(ConstNbr)
    {
        this->setFlags(this->flags() | Qt::ItemIsEditable);       
    }
    ~ConstraintItem()
    {
    }
    void setData(int role, const QVariant & value)
    {
        if (role == Qt::EditRole)
            this->value = value;

        QListWidgetItem::setData(role, value);
    }

    QVariant data (int role) const
    {
        if (ConstraintNbr < 0 || ConstraintNbr >= sketch->Constraints.getSize())
            return QVariant();

        const Sketcher::Constraint * constraint = sketch->Constraints[ConstraintNbr];

        if (role == Qt::EditRole) {
            if (value.isValid())
                return value;
            else
                return Base::Tools::fromStdString(Sketcher::PropertyConstraintList::getConstraintName(constraint->Name, ConstraintNbr));
        }
        else if (role == Qt::DisplayRole) {
            QString name = Base::Tools::fromStdString(Sketcher::PropertyConstraintList::getConstraintName(constraint->Name, ConstraintNbr));

            switch (constraint->Type) {
            case Sketcher::Horizontal:
            case Sketcher::Vertical:
            case Sketcher::Coincident:
            case Sketcher::PointOnObject:
            case Sketcher::Parallel:
            case Sketcher::Perpendicular:
            case Sketcher::Tangent:
            case Sketcher::Equal:
            case Sketcher::Symmetric:
                break;
            case Sketcher::Distance:
                name = QString::fromLatin1("%1 (%2)").arg(name).arg(Base::Quantity(constraint->getValue(),Base::Unit::Length).getUserString());
                break;
            case Sketcher::DistanceX:
                name = QString::fromLatin1("%1 (%2)").arg(name).arg(Base::Quantity(std::abs(constraint->getValue()),Base::Unit::Length).getUserString());
                break;
            case Sketcher::DistanceY:
                name = QString::fromLatin1("%1 (%2)").arg(name).arg(Base::Quantity(std::abs(constraint->getValue()),Base::Unit::Length).getUserString());
                break;
            case Sketcher::Radius:
                name = QString::fromLatin1("%1 (%2)").arg(name).arg(Base::Quantity(constraint->getValue(),Base::Unit::Length).getUserString());
                break;
            case Sketcher::Angle:
                name = QString::fromLatin1("%1 (%2)").arg(name).arg(Base::Quantity(Base::toDegrees<double>(std::abs(constraint->getValue())),Base::Unit::Angle).getUserString());
                break;
            case Sketcher::SnellsLaw: {
                double v = constraint->getValue();
                double n1 = 1.0;
                double n2 = 1.0;
                if (fabs(v) >= 1) {
                    n2 = v;
                } else {
                    n1 = 1/v;
                }
                name = QString::fromLatin1("%1 (%2/%3)").arg(name).arg(n2).arg(n1);
                break;
            }
            case Sketcher::InternalAlignment:
                break;
            default:
                break;
            }
            return name;
        }
        else if (role == Qt::DecorationRole) {
            static QIcon hdist( Gui::BitmapFactory().pixmap("Constraint_HorizontalDistance") );
            static QIcon vdist( Gui::BitmapFactory().pixmap("Constraint_VerticalDistance") );
            static QIcon horiz( Gui::BitmapFactory().pixmap("Constraint_Horizontal") );
            static QIcon vert ( Gui::BitmapFactory().pixmap("Constraint_Vertical") );
            static QIcon lock ( Gui::BitmapFactory().pixmap("Sketcher_ConstrainLock") );
            static QIcon coinc( Gui::BitmapFactory().pixmap("Constraint_PointOnPoint") );
            static QIcon para ( Gui::BitmapFactory().pixmap("Constraint_Parallel") );
            static QIcon perp ( Gui::BitmapFactory().pixmap("Constraint_Perpendicular") );
            static QIcon tang ( Gui::BitmapFactory().pixmap("Constraint_Tangent") );
            static QIcon dist ( Gui::BitmapFactory().pixmap("Constraint_Length") );
            static QIcon radi ( Gui::BitmapFactory().pixmap("Constraint_Radius") );
            static QIcon majradi ( Gui::BitmapFactory().pixmap("Constraint_Ellipse_Major_Radius") );
            static QIcon minradi ( Gui::BitmapFactory().pixmap("Constraint_Ellipse_Minor_Radius") );
            static QIcon angl ( Gui::BitmapFactory().pixmap("Constraint_InternalAngle") );
            static QIcon ellipseXUAngl ( Gui::BitmapFactory().pixmap("Constraint_Ellipse_Axis_Angle") );
            static QIcon equal( Gui::BitmapFactory().pixmap("Constraint_EqualLength") );
            static QIcon pntoo( Gui::BitmapFactory().pixmap("Constraint_PointOnObject") );
            static QIcon symm ( Gui::BitmapFactory().pixmap("Constraint_Symmetric") );
            static QIcon snell ( Gui::BitmapFactory().pixmap("Constraint_SnellsLaw") );
            static QIcon iaellipseminoraxis ( Gui::BitmapFactory().pixmap("Constraint_InternalAlignment_Ellipse_MinorAxis") );
            static QIcon iaellipsemajoraxis ( Gui::BitmapFactory().pixmap("Constraint_InternalAlignment_Ellipse_MajorAxis") );
            static QIcon iaellipsefocus1 ( Gui::BitmapFactory().pixmap("Constraint_InternalAlignment_Ellipse_Focus1") );
            static QIcon iaellipsefocus2 ( Gui::BitmapFactory().pixmap("Constraint_InternalAlignment_Ellipse_Focus2") );
            static QIcon iaellipseother ( Gui::BitmapFactory().pixmap("Constraint_InternalAlignment") );

            static QIcon hdist_driven ( Gui::BitmapFactory().pixmap("Constraint_HorizontalDistance_Driven") );
            static QIcon vdist_driven( Gui::BitmapFactory().pixmap("Constraint_VerticalDistance_Driven") );
            static QIcon dist_driven ( Gui::BitmapFactory().pixmap("Constraint_Length_Driven") );
            static QIcon radi_driven ( Gui::BitmapFactory().pixmap("Constraint_Radius_Driven") );
            static QIcon angl_driven ( Gui::BitmapFactory().pixmap("Constraint_InternalAngle_Driven") );
            static QIcon snell_driven ( Gui::BitmapFactory().pixmap("Constraint_SnellsLaw_Driven") );

            switch(constraint->Type){
            case Sketcher::Horizontal:
                return horiz;
            case Sketcher::Vertical:
                return vert;
            case Sketcher::Coincident:
                return coinc;
            case Sketcher::PointOnObject:
                return pntoo;
            case Sketcher::Parallel:
                return para;
            case Sketcher::Perpendicular:
                return perp;
            case Sketcher::Tangent:
                return tang;
            case Sketcher::Equal:
                return equal;
            case Sketcher::Symmetric:
                return symm;
            case Sketcher::Distance:
                return constraint->isDriving ? dist : dist_driven;
            case Sketcher::DistanceX:
                return constraint->isDriving ? hdist : hdist_driven;
            case Sketcher::DistanceY:
                return constraint->isDriving ? vdist : vdist_driven;
            case Sketcher::Radius:
                return constraint->isDriving ? radi : radi_driven;
            case Sketcher::Angle:
                return constraint->isDriving ? angl : angl_driven;
            case Sketcher::SnellsLaw:
                return constraint->isDriving ? snell : snell_driven;
            case Sketcher::InternalAlignment:
                switch(constraint->AlignmentType){
                case Sketcher::EllipseMajorDiameter:
                    return iaellipsemajoraxis;
                case Sketcher::EllipseMinorDiameter:
                    return iaellipseminoraxis;
                case Sketcher::EllipseFocus1:
                    return iaellipsefocus1;
                case Sketcher::EllipseFocus2:
                    return iaellipsefocus2;
                case Sketcher::Undef:
                default:
                    return iaellipseother;
                }
            default:
                return QVariant();
            }
        }
        else if (role == Qt::ToolTipRole) {
            App::ObjectIdentifier path = sketch->Constraints.createPath(ConstraintNbr);
            App::PropertyExpressionEngine::ExpressionInfo expr_info = sketch->getExpression(path);

            if (expr_info.expression)
                return Base::Tools::fromStdString(expr_info.expression->toString());
            else
                return QVariant();
        }
        else
            return QListWidgetItem::data(role);
    }

    Sketcher::ConstraintType constraintType() const {
        assert(ConstraintNbr >= 0 && ConstraintNbr < sketch->Constraints.getSize());
        return sketch->Constraints[ConstraintNbr]->Type;
    }

    bool isEnforceable() const {
        assert(ConstraintNbr >= 0 && ConstraintNbr < sketch->Constraints.getSize());

        const Sketcher::Constraint * constraint = sketch->Constraints[ConstraintNbr];

        switch (constraint->Type) {
        case Sketcher::None:
            assert( false );
            return false;
        case Sketcher::Horizontal:
        case Sketcher::Vertical:
        case Sketcher::Coincident:
        case Sketcher::PointOnObject:
        case Sketcher::Parallel:
        case Sketcher::Perpendicular:
        case Sketcher::Tangent:
        case Sketcher::Equal:
        case Sketcher::Symmetric:
            return true;
        case Sketcher::Distance:
        case Sketcher::DistanceX:
        case Sketcher::DistanceY:
        case Sketcher::Radius:
        case Sketcher::Angle:
        case Sketcher::SnellsLaw:
            return ( constraint->First >= 0 || constraint->Second >= 0 || constraint->Third >= 0 );
        case Sketcher::InternalAlignment:
            return true;
        }
        return false;
    }

    bool isDriving() const {
        assert(ConstraintNbr >= 0 && ConstraintNbr < sketch->Constraints.getSize());

        return sketch->Constraints[ConstraintNbr]->isDriving;
    }

    const Sketcher::SketchObject * sketch;
    int ConstraintNbr;
    QVariant value;
};

class ExpressionDelegate : public QStyledItemDelegate
{
public:
    ExpressionDelegate(QListWidget * _view) : view(_view) { }
protected:
    QPixmap getIcon(const char* name, const QSize& size) const
    {
        QString key = QString::fromAscii("%1_%2x%3")
            .arg(QString::fromAscii(name))
            .arg(size.width())
            .arg(size.height());
        QPixmap icon;
        if (QPixmapCache::find(key, icon))
            return icon;

        icon = Gui::BitmapFactory().pixmapFromSvg(name, size);
        if (!icon.isNull())
            QPixmapCache::insert(key, icon);
        return icon;
    }

    void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const {
        QStyleOptionViewItemV4 options = option;
        initStyleOption(&options, index);

        options.widget->style()->drawControl(QStyle::CE_ItemViewItem, &options, painter);

        ConstraintItem * item = dynamic_cast<ConstraintItem*>(view->item(index.row()));
        App::ObjectIdentifier path = item->sketch->Constraints.createPath(item->ConstraintNbr);
        App::PropertyExpressionEngine::ExpressionInfo expr_info = item->sketch->getExpression(path);

        if (item->sketch->Constraints[item->ConstraintNbr]->isDriving && expr_info.expression) {
            // Paint pixmap
            int s = 2 * options.rect.height() / 4;
            int margin = s;
            QPixmap pixmap = getIcon(":/icons/bound-expression.svg", QSize(s, s));
            QRect r(options.rect);

            r.setTop(r.top() + (r.height() - s) / 2);
            r.setLeft(r.right() - s);
            r.setHeight(s);
            r.moveLeft(r.left() - margin);
            painter->drawPixmap(r, pixmap);
        }
    }

    QListWidget * view;
};

ConstraintView::ConstraintView(QWidget *parent)
    : QListWidget(parent)
{
    ExpressionDelegate * delegate = new ExpressionDelegate(this);
    setItemDelegate(delegate);
}

ConstraintView::~ConstraintView()
{
}

void ConstraintView::contextMenuEvent (QContextMenuEvent* event)
{
    QMenu menu;
    QListWidgetItem* item = currentItem();
    QList<QListWidgetItem *> items = selectedItems();
    
    CONTEXT_ITEM("Sketcher_SelectElementsAssociatedWithConstraints","Select Elements","Sketcher_SelectElementsAssociatedWithConstraints",doSelectConstraints,true)
    
    menu.addSeparator();
       
    if(item) // Non-driving-constraints/measurements
    {
        ConstraintItem *it = dynamic_cast<ConstraintItem*>(item);
        
        QAction* driven = menu.addAction(tr("Toggle to/from reference"), this, SLOT(updateDrivingStatus()));
        // if its the right constraint
        if ((it->constraintType() == Sketcher::Distance ||
             it->constraintType() == Sketcher::DistanceX ||
             it->constraintType() == Sketcher::DistanceY ||
             it->constraintType() == Sketcher::Radius ||
             it->constraintType() == Sketcher::Angle ||
             it->constraintType() == Sketcher::SnellsLaw) && it->isEnforceable()) {
            driven->setEnabled(true);    
        }
        else{
            driven->setEnabled(false);   
        }
         

        QAction* change = menu.addAction(tr("Change value"), this, SLOT(modifyCurrentItem()));
        QVariant v = item ? item->data(Qt::UserRole) : QVariant();
        change->setEnabled(v.isValid() && it->isDriving());

    }
    QAction* rename = menu.addAction(tr("Rename"), this, SLOT(renameCurrentItem())
#ifndef Q_WS_MAC // on Mac F2 doesn't seem to trigger an edit signal
        ,QKeySequence(Qt::Key_F2)
#endif
        );
    rename->setEnabled(item != 0);

    QAction* center = menu.addAction(tr("Center sketch"), this, SLOT(centerSelectedItems()));
    center->setEnabled(item != 0);

    QAction* remove = menu.addAction(tr("Delete"), this, SLOT(deleteSelectedItems()),
        QKeySequence(QKeySequence::Delete));
    remove->setEnabled(!items.isEmpty());

    QAction* swap = menu.addAction(tr("Swap constraint names"), this, SLOT(swapNamedOfSelectedItems()));
    swap->setEnabled(items.size() == 2);

    menu.exec(event->globalPos());
}

CONTEXT_MEMBER_DEF("Sketcher_SelectElementsAssociatedWithConstraints",doSelectConstraints)

void ConstraintView::updateDrivingStatus()
{
    QListWidgetItem* item = currentItem();
    
    if (item){
        ConstraintItem *it = dynamic_cast<ConstraintItem*>(item);
        
        onUpdateDrivingStatus(item, !it->isDriving());
    }    
}

void ConstraintView::modifyCurrentItem()
{
    /*emit*/itemActivated(currentItem());
}

void ConstraintView::renameCurrentItem()
{
    // See also TaskSketcherConstrains::on_listWidgetConstraints_itemChanged
    QListWidgetItem* item = currentItem();
    if (item)
        editItem(item);
}

void ConstraintView::centerSelectedItems()
{
    Q_EMIT emitCenterSelectedItems();
}

void ConstraintView::deleteSelectedItems()
{
    App::Document* doc = App::GetApplication().getActiveDocument();
    if (!doc) return;

    doc->openTransaction("Delete");
    std::vector<Gui::SelectionObject> sel = Gui::Selection().getSelectionEx(doc->getName());
    for (std::vector<Gui::SelectionObject>::iterator ft = sel.begin(); ft != sel.end(); ++ft) {
        Gui::ViewProvider* vp = Gui::Application::Instance->getViewProvider(ft->getObject());
        if (vp) {
            vp->onDelete(ft->getSubNames());
        }
    }
    doc->commitTransaction();
}

void ConstraintView::swapNamedOfSelectedItems()
{
    QList<QListWidgetItem *> items = selectedItems();

    if (items.size() != 2)
        return;

    ConstraintItem * item1 = static_cast<ConstraintItem*>(items[0]);
    std::string escapedstr1 = Base::Tools::escapedUnicodeFromUtf8(item1->sketch->Constraints[item1->ConstraintNbr]->Name.c_str());
    ConstraintItem * item2 = static_cast<ConstraintItem*>(items[1]);
    std::string escapedstr2 = Base::Tools::escapedUnicodeFromUtf8(item2->sketch->Constraints[item2->ConstraintNbr]->Name.c_str());
    std::stringstream ss;

    ss << "DummyConstraint" << rand();
    std::string tmpname = ss.str();

    Gui::Command::openCommand("Swap constraint names");
    Gui::Command::doCommand(Gui::Command::Doc,"App.ActiveDocument.%s.renameConstraint(%d, u'%s')",
                            item1->sketch->getNameInDocument(),
                            item1->ConstraintNbr, tmpname.c_str());
    Gui::Command::doCommand(Gui::Command::Doc,"App.ActiveDocument.%s.renameConstraint(%d, u'%s')",
                            item2->sketch->getNameInDocument(),
                            item2->ConstraintNbr, escapedstr1.c_str());
    Gui::Command::doCommand(Gui::Command::Doc,"App.ActiveDocument.%s.renameConstraint(%d, u'%s')",
                            item1->sketch->getNameInDocument(),
                            item1->ConstraintNbr, escapedstr2.c_str());
    Gui::Command::commitCommand();
}

// ----------------------------------------------------------------------------

TaskSketcherConstrains::TaskSketcherConstrains(ViewProviderSketch *sketchView)
    : TaskBox(Gui::BitmapFactory().pixmap("document-new"),tr("Constraints"),true, 0)
    , sketchView(sketchView), inEditMode(false)
{
    // we need a separate container widget to add all controls to
    proxy = new QWidget(this);
    ui = new Ui_TaskSketcherConstrains();
    ui->setupUi(proxy);
    ui->listWidgetConstraints->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->listWidgetConstraints->setEditTriggers(QListWidget::EditKeyPressed);
    //QMetaObject::connectSlotsByName(this);

    // connecting the needed signals
    QObject::connect(
        ui->comboBoxFilter, SIGNAL(currentIndexChanged(int)),
        this              , SLOT  (on_comboBoxFilter_currentIndexChanged(int))
       );
    QObject::connect(
        ui->listWidgetConstraints, SIGNAL(itemSelectionChanged()),
        this                     , SLOT  (on_listWidgetConstraints_itemSelectionChanged())
       );
    QObject::connect(
        ui->listWidgetConstraints, SIGNAL(itemActivated(QListWidgetItem *)),
        this                     , SLOT  (on_listWidgetConstraints_itemActivated(QListWidgetItem *))
       );
    QObject::connect(
        ui->listWidgetConstraints, SIGNAL(itemChanged(QListWidgetItem *)),
        this                     , SLOT  (on_listWidgetConstraints_itemChanged(QListWidgetItem *))
       );
    QObject::connect(
        ui->listWidgetConstraints, SIGNAL(emitCenterSelectedItems()),
        this                     , SLOT  (on_listWidgetConstraints_emitCenterSelectedItems())
       );
    QObject::connect(
        ui->listWidgetConstraints, SIGNAL(onUpdateDrivingStatus(QListWidgetItem *, bool)),
        this                     , SLOT  (on_listWidgetConstraints_updateDrivingStatus(QListWidgetItem *, bool))
       );

    connectionConstraintsChanged = sketchView->signalConstraintsChanged.connect(
        boost::bind(&SketcherGui::TaskSketcherConstrains::slotConstraintsChanged, this));

    this->groupLayout()->addWidget(proxy);

    slotConstraintsChanged();
}

TaskSketcherConstrains::~TaskSketcherConstrains()
{
    connectionConstraintsChanged.disconnect();
    delete ui;
}

void TaskSketcherConstrains::onSelectionChanged(const Gui::SelectionChanges& msg)
{
    std::string temp;
    if (msg.Type == Gui::SelectionChanges::ClrSelection) {
        ui->listWidgetConstraints->blockSignals(true);
        ui->listWidgetConstraints->clearSelection ();
        ui->listWidgetConstraints->blockSignals(false);
    }
    else if (msg.Type == Gui::SelectionChanges::AddSelection ||
             msg.Type == Gui::SelectionChanges::RmvSelection) {
        bool select = (msg.Type == Gui::SelectionChanges::AddSelection);
        // is it this object??
        if (strcmp(msg.pDocName,sketchView->getSketchObject()->getDocument()->getName())==0 &&
            strcmp(msg.pObjectName,sketchView->getSketchObject()->getNameInDocument())== 0) {
            if (msg.pSubName) {
                QRegExp rx(QString::fromAscii("^Constraint(\\d+)$"));
                QString expr = QString::fromAscii(msg.pSubName);
                int pos = expr.indexOf(rx);
                if (pos > -1) {
                    bool ok;
                    int ConstrId = rx.cap(1).toInt(&ok) - 1;
                    if (ok) {
                        int countItems = ui->listWidgetConstraints->count();
                        for (int i=0; i < countItems; i++) {
                            ConstraintItem* item = static_cast<ConstraintItem*>
                                (ui->listWidgetConstraints->item(i));
                            if (item->ConstraintNbr == ConstrId) {
                                ui->listWidgetConstraints->blockSignals(true);
                                item->setSelected(select);
                                ui->listWidgetConstraints->blockSignals(false);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    else if (msg.Type == Gui::SelectionChanges::SetSelection) {
        // do nothing here
    }
}

void TaskSketcherConstrains::on_comboBoxFilter_currentIndexChanged(int)
{
    slotConstraintsChanged();
}

void TaskSketcherConstrains::on_listWidgetConstraints_emitCenterSelectedItems()
{
    sketchView->centerSelection();
}

void TaskSketcherConstrains::on_listWidgetConstraints_itemSelectionChanged(void)
{
    std::string doc_name = sketchView->getSketchObject()->getDocument()->getName();
    std::string obj_name = sketchView->getSketchObject()->getNameInDocument();

    bool block = this->blockConnection(true); // avoid to be notified by itself
    Gui::Selection().clearSelection();
    QList<QListWidgetItem *> items = ui->listWidgetConstraints->selectedItems();
    for (QList<QListWidgetItem *>::iterator it = items.begin(); it != items.end(); ++it) {
        std::string constraint_name(Sketcher::PropertyConstraintList::getConstraintName(static_cast<ConstraintItem*>(*it)->ConstraintNbr));

        Gui::Selection().addSelection(doc_name.c_str(), obj_name.c_str(), constraint_name.c_str());
    }
    this->blockConnection(block);
}

void TaskSketcherConstrains::on_listWidgetConstraints_itemActivated(QListWidgetItem *item)
{
    ConstraintItem *it = dynamic_cast<ConstraintItem*>(item);
    if (!item) return;

    // if its the right constraint
    if (it->constraintType() == Sketcher::Distance ||
            it->constraintType() == Sketcher::DistanceX ||
            it->constraintType() == Sketcher::DistanceY ||
            it->constraintType() == Sketcher::Radius ||
            it->constraintType() == Sketcher::Angle ||
            it->constraintType() == Sketcher::SnellsLaw) {

        EditDatumDialog *editDatumDialog = new EditDatumDialog(this->sketchView, it->ConstraintNbr);
        editDatumDialog->exec(false);
        delete editDatumDialog;
    }
}

void TaskSketcherConstrains::on_listWidgetConstraints_updateDrivingStatus(QListWidgetItem *item, bool status)
{
    ConstraintItem *citem = dynamic_cast<ConstraintItem*>(item);
    if (!citem) return;

    Gui::Application::Instance->commandManager().runCommandByName("Sketcher_ToggleDrivingConstraint");
    slotConstraintsChanged();
}

void TaskSketcherConstrains::on_listWidgetConstraints_itemChanged(QListWidgetItem *item)
{
    if (!item || inEditMode)
        return;

    inEditMode = true;

    const ConstraintItem *it = dynamic_cast<const ConstraintItem*>(item);
    const Sketcher::SketchObject * sketch = sketchView->getSketchObject();
    const std::vector< Sketcher::Constraint * > &vals = sketch->Constraints.getValues();
    const Sketcher::Constraint* v = vals[it->ConstraintNbr];
    const std::string currConstraintName = v->Name;

    std::string newName(Sketcher::PropertyConstraintList::getConstraintName(Base::Tools::toStdString(it->data(Qt::EditRole).toString()), it->ConstraintNbr));

    if (newName != currConstraintName) {
        std::string escapedstr = Base::Tools::escapedUnicodeFromUtf8(newName.c_str());

        Gui::Command::openCommand("Rename sketch constraint");
        try {
            Gui::Command::doCommand(Gui::Command::Doc,"App.ActiveDocument.%s.renameConstraint(%d, u'%s')",
                                    sketch->getNameInDocument(),
                                    it->ConstraintNbr, escapedstr.c_str());
            Gui::Command::commitCommand();
        }
        catch (const Base::Exception & e) {
            Gui::Command::abortCommand();

            QMessageBox::critical(Gui::MainWindow::getInstance(), QString::fromAscii("Error"),
                                  QString::fromAscii(e.what()), QMessageBox::Ok, QMessageBox::Ok);
        }
    }

    inEditMode = false;
}

void TaskSketcherConstrains::slotConstraintsChanged(void)
{
    assert(sketchView);
    // Build up ListView with the constraints
    const Sketcher::SketchObject * sketch = sketchView->getSketchObject();
    const std::vector< Sketcher::Constraint * > &vals = sketch->Constraints.getValues();

    /* Update constraint number */
    for (int i = 0; i <  ui->listWidgetConstraints->count(); ++i) {
        ConstraintItem * it = dynamic_cast<ConstraintItem*>(ui->listWidgetConstraints->item(i));

        assert(it != 0);

        it->ConstraintNbr = i;
        it->value = QVariant();
    }

    /* Remove entries, if any */
    for (std::size_t i = ui->listWidgetConstraints->count(); i > vals.size(); --i)
        delete ui->listWidgetConstraints->takeItem(i - 1);

    /* Add new entries, if any */
    for (std::size_t i = ui->listWidgetConstraints->count(); i < vals.size(); ++i)
        ui->listWidgetConstraints->addItem(new ConstraintItem(sketch, i));

    /* Update filtering */
    int Filter = ui->comboBoxFilter->currentIndex();
    for(std::size_t i = 0; i < vals.size(); ++i) {
        const Sketcher::Constraint * constraint = vals[i];
        ConstraintItem * it = static_cast<ConstraintItem*>(ui->listWidgetConstraints->item(i));
        bool visible = true;

        /* Filter
         0 <=> All
         1 <=> Normal
         2 <=> Datums
         3 <=> Named
         4 <=> Non-Driving
        */

        bool showNormal = (Filter < 2);
        bool showDatums = (Filter < 3);
        bool showNamed = (Filter == 3 && !(constraint->Name.empty()));
        bool showNonDriving = (Filter == 4 && !constraint->isDriving);

        switch(constraint->Type) {
        case Sketcher::Horizontal:
        case Sketcher::Vertical:
        case Sketcher::Coincident:
        case Sketcher::PointOnObject:
        case Sketcher::Parallel:
        case Sketcher::Perpendicular:
        case Sketcher::Tangent:
        case Sketcher::Equal:
        case Sketcher::Symmetric:
            visible = showNormal || showNamed;
            break;
        case Sketcher::Distance:
        case Sketcher::DistanceX:
        case Sketcher::DistanceY:
        case Sketcher::Radius:
        case Sketcher::Angle:
        case Sketcher::SnellsLaw:
            visible = (showDatums || showNamed || showNonDriving);
            break;
        case Sketcher::InternalAlignment:
            visible = (showNormal || showNamed);
        default:
            break;
        }

        it->setHidden(!visible);
        it->setData(Qt::EditRole, Base::Tools::fromStdString(constraint->Name));
    }
}

void TaskSketcherConstrains::changeEvent(QEvent *e)
{
    TaskBox::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        ui->retranslateUi(proxy);
    }
}



#include "moc_TaskSketcherConstrains.cpp"
