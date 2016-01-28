/******************************************************************************
*       SOFA, Simulation Open-Framework Architecture, development version     *
*                (c) 2006-2015 INRIA, USTL, UJF, CNRS, MGH                    *
*                                                                             *
* This library is free software; you can redistribute it and/or modify it     *
* under the terms of the GNU Lesser General Public License as published by    *
* the Free Software Foundation; either version 2.1 of the License, or (at     *
* your option) any later version.                                             *
*                                                                             *
* This library is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License *
* for more details.                                                           *
*                                                                             *
* You should have received a copy of the GNU Lesser General Public License    *
* along with this library; if not, write to the Free Software Foundation,     *
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.          *
*******************************************************************************
*                               SOFA :: Modules                               *
*                                                                             *
* Authors: The SOFA Team and external contributors (see Authors.txt)          *
*                                                                             *
* Contact information: contact@sofa-framework.org                             *
******************************************************************************/
// Author: François Faure, INRIA-UJF, (C) 2006
#define SOFA_COMPONENT_LINEARSOLVER_CGLINEARSOLVER_CPP
#include <SofaBaseLinearSolver/CGLinearSolver.inl>

#include <sofa/core/ObjectFactory.h>
#include <iostream>

namespace sofa
{

namespace component
{

namespace linearsolver
{

using namespace sofa::defaulttype;
using sofa::core::MultiVecDerivId;

template<> SOFA_BASE_LINEAR_SOLVER_API
inline void CGLinearSolver<component::linearsolver::GraphScatteredMatrix,component::linearsolver::GraphScatteredVector>::cgstep_beta(const core::ExecParams* /*params*/, Vector& p, Vector& r, SReal beta)
{
    p.eq(r,p,beta); // p = p*beta + r
}

/// Solve Mx=b
template<>
void CGLinearSolver< CompressedRowSparseMatrix<double>, FullVector<double> >::solve(CompressedRowSparseMatrix<double>& M, FullVector<double>& x, FullVector<double>& b)
{
    //std::cout<<"CGLinearSolver< CompressedRowSparseMatrix<double>, FullVector<double> >::solve "<<std::endl;
#ifdef SOFA_DUMP_VISITOR_INFO
    simulation::Visitor::printComment("ConjugateGradient");
#endif

#ifdef SOFA_DUMP_VISITOR_INFO
    simulation::Visitor::printNode("VectorAllocation");
#endif
    const core::ExecParams* params = core::ExecParams::defaultInstance();
    typename Inherit::TempVectorContainer vtmp(this, params, M, x, b);
    Vector& p = *vtmp.createTempVector();
    Vector& q = *vtmp.createTempVector();
    Vector& r = *vtmp.createTempVector();

    const bool printLog = this->f_printLog.getValue();
    const bool verbose  = f_verbose.getValue();

    // -- solve the system using a conjugate gradient solution
    double rho, rho_1=0, alpha, beta;

    sofa::component::linearsolver::CompressedRowSparseMatrix<SReal> Mfiltered;
    Mfiltered.copyNonZeros(M);


//    std::ofstream myfile ("M.m",std::ios::trunc);
//    const double ZERO = 1e-16;
//    myfile<<"Afull = zeros("<<Mfiltered.nRow<<");" <<std::endl;
//    for(int r=0; r< Mfiltered.nRow; ++r)// n: number of column
//    {
//        for(int c=0; c< Mfiltered.nCol; ++c)// n: number of column
//        {
//            if(Mfiltered.element(r,c)>ZERO)
//                myfile <<"Afull(" <<r+1<<","<<c+1<< ")="<< Mfiltered.element(r,c)<<";" <<std::endl;
//        }
//    }

//    myfile << "bfull=[";

//    for(int q=0; q<b.size(); ++q)
//    {
//        myfile << b[q] <<" ";
//    }
//    myfile << "];"<<std::endl;

//    myfile <<"S=sparse(Afull);"<<std::endl;

//    myfile<<"tic"<<std::endl;
//    myfile<<"x=cgs(S,bfull',"<<f_tolerance.getValue()<<");"<<std::endl;
//    myfile<<"toc"<<std::endl;

//    myfile.close();

    if( verbose )
        sout<<"CGLinearSolver, b = "<< b <<sendl;

    if( f_warmStart.getValue() )
    {
        r = M * x;
        r.eq( b, r, -1.0 );   //  initial residual r = b - Ax;
    }
    else
    {
        x.clear();
        r = b; // initial residual
    }

    double normb = b.norm();
    std::map < std::string, sofa::helper::vector<SReal> >& graph = *f_graph.beginEdit();
    sofa::helper::vector<SReal>& graph_error = graph[(this->isMultiGroup()) ? this->currentNode->getName()+std::string("-Error") : std::string("Error")];
    graph_error.clear();
    sofa::helper::vector<SReal>& graph_den = graph[(this->isMultiGroup()) ? this->currentNode->getName()+std::string("-Denominator") : std::string("Denominator")];
    graph_den.clear();
    graph_error.push_back(1);
    unsigned nb_iter;
    const char* endcond = "iterations";

#ifdef DISPLAY_TIME
    sofa::helper::system::thread::CTime timer;
    time1 = (SReal) timer.getTime();
#endif

#ifdef SOFA_DUMP_VISITOR_INFO
    simulation::Visitor::printCloseNode("VectorAllocation");
#endif
    for( nb_iter=1; nb_iter<=f_maxIter.getValue(); nb_iter++ )
    {
#ifdef SOFA_DUMP_VISITOR_INFO
        std::ostringstream comment;
        if (simulation::Visitor::isPrintActivated())
        {
            comment << "Iteration_" << nb_iter;
            simulation::Visitor::printNode(comment.str());
        }
#endif
        // 		printWithElapsedTime( x, helper::system::thread::CTime::getTime()-time0,sout );

        //z = r; // no precond
        //rho = r.dot(z);
        rho = r.dot(r);

        if (nb_iter>1)
        {
            double normr = sqrt(rho); //sqrt(r.dot(r));
            double err = normr/normb;
            graph_error.push_back(err);
            if (err <= f_tolerance.getValue())
            {
                endcond = "tolerance";

#ifdef SOFA_DUMP_VISITOR_INFO
                if (simulation::Visitor::isPrintActivated())
                    simulation::Visitor::printCloseNode(comment.str());
#endif
                break;
            }
        }

        if( nb_iter==1 )
            p = r; //z;
        else
        {
            beta = rho / rho_1;
            //p = p*beta + r; //z;
            cgstep_beta(params, p,r,beta);
        }

        if( verbose )
        {
            sout<<"p : "<<p<<sendl;
        }

        // matrix-vector product
        q = M*p;

        if( verbose )
        {
            sout<<"q = M p : "<<q<<sendl;
        }

        double den = p.dot(q);

        graph_den.push_back(den);

        if( fabs(den)<f_smallDenominatorThreshold.getValue() )
        {
            endcond = "threshold";
            if( verbose )
            {
                sout<<"CGLinearSolver, den = "<<den<<", smallDenominatorThreshold = "<<f_smallDenominatorThreshold.getValue()<<sendl;
            }
#ifdef SOFA_DUMP_VISITOR_INFO
            if (simulation::Visitor::isPrintActivated())
                simulation::Visitor::printCloseNode(comment.str());
#endif
            break;
        }
        alpha = rho/den;
        //x.peq(p,alpha);                 // x = x + alpha p
        //r.peq(q,-alpha);                // r = r - alpha q
        cgstep_alpha(params, x,r,p,q,alpha);
        if( verbose )
        {
            sout<<"den = "<<den<<", alpha = "<<alpha<<sendl;
            sout<<"x : "<<x<<sendl;
            sout<<"r : "<<r<<sendl;
        }

        rho_1 = rho;
#ifdef SOFA_DUMP_VISITOR_INFO
        if (simulation::Visitor::isPrintActivated())
            simulation::Visitor::printCloseNode(comment.str());
#endif
    }

#ifdef DISPLAY_TIME
    time1 = (SReal)(((SReal) timer.getTime() - time1) * timeStamp / (nb_iter-1));
#endif

    f_graph.endEdit();

    sofa::helper::AdvancedTimer::valSet("CG iterations", nb_iter);

    // x is the solution of the system
    if( printLog )
    {
#ifdef DISPLAY_TIME
        std::cerr<<"CGLinearSolver::solve, CG = "<<time1<<" build = "<<time2<<std::endl;
#endif
        sout<<"CGLinearSolver::solve, nbiter = "<<nb_iter<<" stop because of "<<endcond<<sendl;
    }
    if( verbose )
    {
        sout<<"CGLinearSolver::solve, solution = "<<x<<sendl;
    }
    vtmp.deleteTempVector(&p);
    vtmp.deleteTempVector(&q);
    vtmp.deleteTempVector(&r);
}

template<> SOFA_BASE_LINEAR_SOLVER_API
inline void CGLinearSolver<component::linearsolver::GraphScatteredMatrix,component::linearsolver::GraphScatteredVector>::cgstep_alpha(const core::ExecParams* params, Vector& x, Vector& r, Vector& p, Vector& q, SReal alpha)
{
#ifdef SOFA_NO_VMULTIOP // unoptimized version
    x.peq(p,alpha);                 // x = x + alpha p
    r.peq(q,-alpha);                // r = r - alpha q
#else // single-operation optimization
    typedef sofa::core::behavior::BaseMechanicalState::VMultiOp VMultiOp;
    VMultiOp ops;
    ops.resize(2);
    ops[0].first = (MultiVecDerivId)x;
    ops[0].second.push_back(std::make_pair((MultiVecDerivId)x,1.0));
    ops[0].second.push_back(std::make_pair((MultiVecDerivId)p,alpha));
    ops[1].first = (MultiVecDerivId)r;
    ops[1].second.push_back(std::make_pair((MultiVecDerivId)r,1.0));
    ops[1].second.push_back(std::make_pair((MultiVecDerivId)q,-alpha));
    this->executeVisitor(simulation::MechanicalVMultiOpVisitor(params, ops));
#endif
}

SOFA_DECL_CLASS(CGLinearSolver)

int CGLinearSolverClass = core::RegisterObject("Linear system solver using the conjugate gradient iterative algorithm")
        .add< CGLinearSolver< GraphScatteredMatrix, GraphScatteredVector > >(true)
#ifndef SOFA_FLOAT
        .add< CGLinearSolver< FullMatrix<double>, FullVector<double> > >()
        .add< CGLinearSolver< SparseMatrix<double>, FullVector<double> > >()
        .add< CGLinearSolver< CompressedRowSparseMatrix<double>, FullVector<double> > >()
        .add< CGLinearSolver< CompressedRowSparseMatrix<Mat<2,2,double> >, FullVector<double> > >()
        .add< CGLinearSolver< CompressedRowSparseMatrix<Mat<3,3,double> >, FullVector<double> > >()
        .add< CGLinearSolver< CompressedRowSparseMatrix<Mat<4,4,double> >, FullVector<double> > >()
        .add< CGLinearSolver< CompressedRowSparseMatrix<Mat<6,6,double> >, FullVector<double> > >()
        .add< CGLinearSolver< CompressedRowSparseMatrix<Mat<8,8,double> >, FullVector<double> > >()
#endif
#ifndef SOFA_DOUBLE
        .add< CGLinearSolver< CompressedRowSparseMatrix<float>, FullVector<float> > >()
        .add< CGLinearSolver< CompressedRowSparseMatrix<Mat<2,2,float> >, FullVector<float> > >()
        .add< CGLinearSolver< CompressedRowSparseMatrix<Mat<3,3,float> >, FullVector<float> > >()
        .add< CGLinearSolver< CompressedRowSparseMatrix<Mat<4,4,float> >, FullVector<float> > >()
        .add< CGLinearSolver< CompressedRowSparseMatrix<Mat<6,6,float> >, FullVector<float> > >()
        .add< CGLinearSolver< CompressedRowSparseMatrix<Mat<8,8,float> >, FullVector<float> > >()
#endif
        .addAlias("CGSolver")
        .addAlias("ConjugateGradient")
        ;

template class SOFA_BASE_LINEAR_SOLVER_API CGLinearSolver< GraphScatteredMatrix, GraphScatteredVector >;
#ifndef SOFA_FLOAT
template class SOFA_BASE_LINEAR_SOLVER_API CGLinearSolver< FullMatrix<double>, FullVector<double> >;
template class SOFA_BASE_LINEAR_SOLVER_API CGLinearSolver< SparseMatrix<double>, FullVector<double> >;
template class SOFA_BASE_LINEAR_SOLVER_API CGLinearSolver< CompressedRowSparseMatrix<double>, FullVector<double> >;
template class SOFA_BASE_LINEAR_SOLVER_API CGLinearSolver< CompressedRowSparseMatrix<defaulttype::Mat<2,2,double> >, FullVector<double> >;
template class SOFA_BASE_LINEAR_SOLVER_API CGLinearSolver< CompressedRowSparseMatrix<defaulttype::Mat<3,3,double> >, FullVector<double> >;
template class SOFA_BASE_LINEAR_SOLVER_API CGLinearSolver< CompressedRowSparseMatrix<defaulttype::Mat<4,4,double> >, FullVector<double> >;
template class SOFA_BASE_LINEAR_SOLVER_API CGLinearSolver< CompressedRowSparseMatrix<defaulttype::Mat<6,6,double> >, FullVector<double> >;
template class SOFA_BASE_LINEAR_SOLVER_API CGLinearSolver< CompressedRowSparseMatrix<defaulttype::Mat<8,8,double> >, FullVector<double> >;
#endif

#ifndef SOFA_DOUBLE
template class SOFA_BASE_LINEAR_SOLVER_API CGLinearSolver< CompressedRowSparseMatrix<float>, FullVector<float> >;
template class SOFA_BASE_LINEAR_SOLVER_API CGLinearSolver< CompressedRowSparseMatrix<defaulttype::Mat<2,2,float> >, FullVector<float> >;
template class SOFA_BASE_LINEAR_SOLVER_API CGLinearSolver< CompressedRowSparseMatrix<defaulttype::Mat<3,3,float> >, FullVector<float> >;
template class SOFA_BASE_LINEAR_SOLVER_API CGLinearSolver< CompressedRowSparseMatrix<defaulttype::Mat<4,4,float> >, FullVector<float> >;
template class SOFA_BASE_LINEAR_SOLVER_API CGLinearSolver< CompressedRowSparseMatrix<defaulttype::Mat<6,6,float> >, FullVector<float> >;
template class SOFA_BASE_LINEAR_SOLVER_API CGLinearSolver< CompressedRowSparseMatrix<defaulttype::Mat<8,8,float> >, FullVector<float> >;
#endif
} // namespace linearsolver

} // namespace component

} // namespace sofa

