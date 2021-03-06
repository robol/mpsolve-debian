#include "polynomialsolver.h"
#include "mpsolveworker.h"
#include "root.h"
#include "polynomialparser.h"
#include <QDebug>
#include <QRegExp>
#include <QStringList>

using namespace xmpsolve;

PolynomialSolver::PolynomialSolver(QObject *parent) :
    QObject(parent)
{
    m_mpsContext = NULL;
    m_mpsPoly = NULL;
    m_worker.connect(&m_worker, SIGNAL(finished()),
                     this, SLOT(workerExited()));
    m_errorMessage = "";
}

PolynomialSolver::~PolynomialSolver()
{
    if (m_mpsPoly)
        m_mpsPoly->free (m_mpsContext, m_mpsPoly);
    if (m_mpsContext)
        mps_context_free(m_mpsContext);
}

int
PolynomialSolver::solvePolFile(QString selectedFile, mps_algorithm selected_algorithm, int required_digits)
{
    QByteArray stringData = selectedFile.toLatin1().data();

    m_mpsContext = mps_context_new();
    m_worker.setMpsContext(m_mpsContext);

    // Parse the stream specified by the user
    mps_polynomial * poly = mps_parse_file (m_mpsContext, stringData.data());

    if (mps_context_has_errors (m_mpsContext) || !poly) {
        m_errorMessage = tr("Error while solving the given pol file: %1").
                arg(mps_context_error_msg(m_mpsContext));
        mps_context_free (m_mpsContext);
        m_mpsContext = NULL;
        return -1;
    }
    else
        mps_context_set_input_poly (m_mpsContext, poly);

    // Select the options selected by the user
    mps_context_select_algorithm(m_mpsContext, selected_algorithm);
    mps_context_set_output_prec(m_mpsContext, required_digits * LOG2_10);
    mps_context_set_output_goal(m_mpsContext, MPS_OUTPUT_GOAL_APPROXIMATE);

    m_worker.start();
    return mps_context_get_degree (m_mpsContext);
}

int
PolynomialSolver::solvePoly(Polynomial poly, PolynomialBasis basis,
                            mps_algorithm selected_algorithm,
                            int required_digits)
{
    m_currentPoly = poly;

    // Regenerate a new mps_context, since as of now the same cannot be used
    // more than one time.
    if (m_mpsPoly)
        m_mpsPoly->free(m_mpsContext, m_mpsPoly);
    if (m_mpsContext)
        mps_context_free(m_mpsContext);
    m_mpsContext = mps_context_new();

    m_worker.setMpsContext(m_mpsContext);

    m_mpsPoly = NULL;

    switch (basis) {
        case MONOMIAL:
            m_mpsPoly = MPS_POLYNOMIAL (mps_monomial_poly_new(m_mpsContext, poly.degree()));
            for (int i = 0; i <= poly.degree(); i++) {
                poly.monomial(i).addToMonomialPoly(m_mpsContext,
                                                   MPS_MONOMIAL_POLY (m_mpsPoly));
            }
            break;

        case CHEBYSHEV:
            m_mpsPoly = MPS_POLYNOMIAL (mps_chebyshev_poly_new (m_mpsContext, poly.degree (),
                                                               MPS_STRUCTURE_COMPLEX_RATIONAL));
            for (int i = 0; i <= poly.degree(); i++) {
                poly.monomial(i).addToChebyshevPoly(m_mpsContext,
                                                    MPS_CHEBYSHEV_POLY (m_mpsPoly));
            }
    }

    mps_context_set_input_poly(m_mpsContext, m_mpsPoly);
    mps_context_select_algorithm(m_mpsContext, selected_algorithm);
    mps_context_set_output_prec(m_mpsContext, required_digits * LOG2_10);
    mps_context_set_output_goal(m_mpsContext, MPS_OUTPUT_GOAL_APPROXIMATE);

    m_worker.start();
    return mps_context_get_degree (m_mpsContext);
}

int
PolynomialSolver::solvePoly(QString inputString, PolynomialBasis basis,
                            mps_algorithm selected_algorithm,
                            int required_digits)
{
    PolynomialParser parser;

    // Parse the input string that the user has given.
    Polynomial poly = parser.parse(inputString);

    if (poly.degree() != 0) {
        return solvePoly(poly, basis, selected_algorithm, required_digits);
    }
    else {
       m_errorMessage = parser.errorMessage();

       if (m_errorMessage == QString("")) {
           m_errorMessage = tr("Constant polynomials have no roots");
       }

       return -1;
    }
}

QString
PolynomialSolver::errorMessage()
{
    return m_errorMessage;
}

void
PolynomialSolver::workerExited()
{
    int n = mps_context_get_degree (m_mpsContext);

    /* Save the roots in a list */
    QList<Root*> roots;

    /* Prepare some space to save the roots */
    mpc_t * results = new mpc_t[n];
    rdpe_t * radii = new rdpe_t[n];

    mpc_vinit2 (results, n, 53);

    mps_context_get_roots_m(m_mpsContext, &results, &radii);

    for (int i = 0; i < n; i++)
    {
        mps_root_status status = mps_context_get_root_status (m_mpsContext, i);
        Root* r = new Root(results[i], radii[i], status);
        roots.append(r);
    }

    for (int i = 0; i < mps_context_get_zero_roots (m_mpsContext); i++)
    {
        Root * r = new Root();

        mpc_init2 (r->value, 0);
        mpc_set_ui (r->value, 0U, 0U);
        rdpe_set (r->radius, rdpe_zero);

        roots.append(r);
    }

    mpc_vclear (results, n);
    delete [] results;
    delete [] radii;

    // Update the internal model with the approximations
    m_rootsModel.setRoots(roots);

    /* Set the roots somewhere and then called
     * the solved() signal */
    solved();
}

unsigned long int
PolynomialSolver::CPUTime()
{
    return m_worker.CPUTime();
}

RootsModel *
PolynomialSolver::rootsModel()
{
    return &m_rootsModel;
}
