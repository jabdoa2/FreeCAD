/***************************************************************************
 *   Copyright (c) Juergen Riegel         <juergen.riegel@web.de>          *
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
#endif

#include <Base/Console.h>
#include <Base/Interpreter.h>
#include <Base/FileInfo.h>

#include <App/Application.h>
#include <App/Document.h>
#include <App/DocumentObject.h>
#include <App/Property.h>

// PCL test
#ifdef HAVE_PCL_IO
#  include <iostream>
#  include <pcl/io/ply_io.h>
#  include <pcl/point_types.h>
#endif

#include "Points.h"
#include "PointsPy.h"
#include "PointsAlgos.h"
#include "FeaturePointsImportAscii.h"

using namespace Points;

/* module functions */
static PyObject *
open(PyObject *self, PyObject *args)
{
    char* Name;
    if (!PyArg_ParseTuple(args, "et","utf-8",&Name))
        return NULL;
    std::string EncodedName = std::string(Name);
    PyMem_Free(Name);

    PY_TRY {
        Base::Console().Log("Open in Points with %s",EncodedName.c_str());
        Base::FileInfo file(EncodedName.c_str());

        // extract ending
        if (file.extension() == "")
            Py_Error(Base::BaseExceptionFreeCADError,"no file ending");

        if (file.hasExtension("asc")) {
            // create new document and add Import feature
            App::Document *pcDoc = App::GetApplication().newDocument("Unnamed");
            Points::Feature *pcFeature = (Points::Feature *)pcDoc->addObject("Points::Feature", file.fileNamePure().c_str());
            Points::PointKernel pkTemp;
            pkTemp.load(EncodedName.c_str());
            pcFeature->Points.setValue( pkTemp );

        }
#ifdef HAVE_PCL_IO
        else if (file.hasExtension("ply")) {
            // create new document import
            App::Document *pcDoc = App::GetApplication().newDocument("Unnamed");
            Points::Feature *pcFeature = (Points::Feature *)pcDoc->addObject("Points::Feature", file.fileNamePure().c_str());
            Points::PointKernel pkTemp;

            // pcl test
            pcl::PointCloud<pcl::PointXYZ> cloud_in;
            pcl::io::loadPLYFile<pcl::PointXYZ>(EncodedName.c_str(),cloud_in); 

            for (pcl::PointCloud<pcl::PointXYZ>::const_iterator it = cloud_in.begin();it!=cloud_in.end();++it)
                pkTemp.push_back(Base::Vector3d(it->x,it->y,it->z));
            pcFeature->Points.setValue( pkTemp );
        }
#endif
        else {
            Py_Error(Base::BaseExceptionFreeCADError,"unknown file ending");
        }
    } PY_CATCH;

    Py_Return;
}

static PyObject *
insert(PyObject *self, PyObject *args)
{
    char* Name;
    const char* DocName;
    if (!PyArg_ParseTuple(args, "ets","utf-8",&Name,&DocName))
        return NULL;
    std::string EncodedName = std::string(Name);
    PyMem_Free(Name);

    PY_TRY {
        Base::Console().Log("Import in Points with %s",EncodedName.c_str());
        Base::FileInfo file(EncodedName.c_str());

        // extract ending
        if (file.extension() == "")
            Py_Error(Base::BaseExceptionFreeCADError,"no file ending");

        if (file.hasExtension("asc")) {
            // add Import feature
            App::Document *pcDoc = App::GetApplication().getDocument(DocName);
            if (!pcDoc) {
                pcDoc = App::GetApplication().newDocument(DocName);
            }

            Points::Feature *pcFeature = (Points::Feature *)pcDoc->addObject("Points::Feature", file.fileNamePure().c_str());
            Points::PointKernel pkTemp;
            pkTemp.load(EncodedName.c_str());
            pcFeature->Points.setValue( pkTemp );
        }
#ifdef HAVE_PCL_IO
        else if (file.hasExtension("ply")) {
            App::Document *pcDoc = App::GetApplication().getDocument(DocName);
            if (!pcDoc) {
                pcDoc = App::GetApplication().newDocument(DocName);
            }

            Points::Feature *pcFeature = (Points::Feature *)pcDoc->addObject("Points::Feature", file.fileNamePure().c_str());
            Points::PointKernel pkTemp;

            // pcl test
            pcl::PointCloud<pcl::PointXYZRGB> cloud_in;
            pcl::io::loadPLYFile<pcl::PointXYZRGB>(EncodedName.c_str(),cloud_in); 

            for (pcl::PointCloud<pcl::PointXYZRGB>::const_iterator it = cloud_in.begin();it!=cloud_in.end();++it)
                pkTemp.push_back(Base::Vector3d(it->x,it->y,it->z));
            pcFeature->Points.setValue( pkTemp );
        }
#endif
        else {
            Py_Error(Base::BaseExceptionFreeCADError,"unknown file ending");
        }
    } PY_CATCH;

    Py_Return;
}

static PyObject * 
show(PyObject *self, PyObject *args)
{
    PyObject *pcObj;
    if (!PyArg_ParseTuple(args, "O!", &(PointsPy::Type), &pcObj))     // convert args: Python->C
        return NULL;                             // NULL triggers exception

    PY_TRY {
        App::Document *pcDoc = App::GetApplication().getActiveDocument(); 	 
        if (!pcDoc)
            pcDoc = App::GetApplication().newDocument();
        PointsPy* pPoints = static_cast<PointsPy*>(pcObj);
        Points::Feature *pcFeature = (Points::Feature *)pcDoc->addObject("Points::Feature", "Points");
        // copy the data
        //TopoShape* shape = new MeshObject(*pShape->getTopoShapeObjectPtr());
        pcFeature->Points.setValue(*(pPoints->getPointKernelPtr()));
        //pcDoc->recompute();
    } PY_CATCH;

    Py_Return;
}

// registration table  
struct PyMethodDef Points_Import_methods[] = {
    {"open",  open,   1},       /* method name, C func ptr, always-tuple */
    {"insert",insert, 1},
    {"show",show, 1},
    {NULL, NULL}                /* end of table marker */
};
