#ifndef TD_FIT_CURVES_HPP
#define TD_FIT_CURVES_HPP


#include "tendril.hpp"

namespace td {
/*
This is based on:

An Algorithm for Automatically Fitting Digitized Curves
by Philip J. Schneider
from "Graphics Gems", Academic Press, 1990

The original file FitCurves.c can be found next to this file.
*/

/*  fit_cubic.c    */                                    
/*    Piecewise cubic fitting code    */


typedef td_vec2 Vector2;
typedef td_vec2 Point2;
//typedef td_vec2* BezierCurve;
typedef td_bezier BezierCurve;
typedef td_vec2* BezierPointsPtr;
/* Forward declarations */

static float V2Dot(Vector2* a, Vector2* b);
static float V2DistanceBetween2Points(Point2* a, Point2* b);

static Vector2 V2AddII(Vector2 a, Vector2 b);
static Vector2 V2ScaleIII(Vector2 v, float s);
static Vector2 V2SubII(Vector2 a, Vector2 b);
static float V2SquaredLength(Vector2* a);
static float V2Length(Vector2* a);
static Vector2* V2Negate(Vector2* v);
static Vector2* V2Normalize(Vector2* v);
static Vector2* V2Scale(Vector2* v, float newlen);
static Vector2* V2Add(Vector2* a, Vector2* b, Vector2* c);

#define MAXPOINTS    1000        /* The most points you can have */

struct fit_curve_op
{
    typedef td_vec2 point;
    typedef td_bezier bezier;

    fit_curve_op()
    {

    }

    td_vector<td_bezier> results;

    /* Parameter values for point.
       Temp allocator. */
    td_vector<float> chord;
    /* Improved parameter values.
       Temp allocator. */
    td_vector<float> uprime;

    void DrawCubicBezierCurve(BezierCurve* curve)
    {
        results.push_back(*curve);
    }

    /*
    *  fit_curve :
    *      Fit a Bezier curve to a set of digitized points
    */

    void fit_curve(
        point* points, /*  Array of digitized points    */
        int count,     /*  Number of digitized points    */
        float error    /*  User-defined error squared    */
    )
    {
        results.clear();
        Vector2 tHat1;    /*  Unit tangent vectors at endpoints */
        Vector2 tHat2;
        tHat1 = ComputeLeftTangent(points, 0);
        tHat2 = ComputeRightTangent(points, count - 1);
        fit_cubic(points, 0, count - 1, tHat1, tHat2, error);
    }

    /*
     *  fit_cubic :
     *      Fit a Bezier curve to a (sub)set of digitized points
     */
    void fit_cubic(
        point*  d,      /*  Array of digitized points */
        int     first,  /* Indices of first and last pts in region */
        int     last,    
        Vector2 tHat1,  /* Unit tangent vectors at endpoints */
        Vector2 tHat2,   
        float  error   /* User-defined error squared */
        )
    {
        bezier bezCurve;          /* Control points of fitted Bezier curve*/
        float maxError;          /* Maximum fitting error     */
        int    splitPoint;        /* Point to split point set at     */
        int    points_count;      /* Number of points in subset  */
        float iterationError;    /* Error below which you try iterating  */
        int    maxIterations = 4; /* Max times to try iterating  */
        int    i;

        iterationError = error * 4.0f;    /* fixed issue 23 */
        points_count = last - first + 1;

        /*  Use heuristic if region only has two points in it */
        if (points_count == 2)
        {
            float dist = V2DistanceBetween2Points(&d[last], &d[first]) / 3.0f;

            bezCurve[0] = d[first];
            bezCurve[3] = d[last];
            V2Add(&bezCurve[0], V2Scale(&tHat1, dist), &bezCurve[1]);
            V2Add(&bezCurve[3], V2Scale(&tHat2, dist), &bezCurve[2]);

            DrawCubicBezierCurve(&bezCurve);

            return;
        }

        /* Allocate approriate space. */
        {
            chord.clear();
            chord.resize(points_count);
            uprime.clear();
            uprime.resize(points_count);
        }

        /* Parameterize points (chord_len), and attempt to fit curve */
        populate_chord_length_param(d, first, last, chord.data());
        generate_bezier(d, first, last, chord.data(), tHat1, tHat2, &bezCurve);

        /*  Find max deviation of points to fitted curve */
        maxError = compute_max_error(d, first, last, bezCurve.ptr(), chord.data(), &splitPoint);
        if (maxError < error) {
            DrawCubicBezierCurve(&bezCurve);
            return;
        }

        /*  If error not too large, try some reparameterization  */
        /*  and iteration */
        if (maxError < iterationError)
        {
            for (i = 0; i < maxIterations; i += 1)
            {
                /*
                *  Reparameterize:
                *    Given set of points and their parameterization,
                *    try to find a better parameterization.
                *  NOTE: Populate the 'uprime' based on 'chord'
                */
                for (int upi = first; upi <= last; upi += 1)
                {
                    uprime[upi - first] = newton_raphson_root_find(bezCurve.ptr(), d[upi], chord[upi - first]);
                }

                generate_bezier(d, first, last, uprime.data(), tHat1, tHat2, &bezCurve);
                maxError = compute_max_error(d, first, last, bezCurve.ptr(), uprime.data(), &splitPoint);
                if (maxError < error)
                {
                    DrawCubicBezierCurve(&bezCurve);
                    return;
                }
            }
        }

        /* Fitting failed -- split at max error point and fit recursively */

        /* Unit tangent vector at splitPoint */
        Vector2 tHatCenter = ComputeCenterTangent(d, splitPoint);
        fit_cubic(d, first, splitPoint, tHat1, tHatCenter, error);
        V2Negate(&tHatCenter);
        fit_cubic(d, splitPoint, last, tHatCenter, tHat2, error);
    }

    /*
     *  GenerateBezier :
     *  Use least-squares method to find Bezier control points for region.
     *
     */
    void generate_bezier(
        point* d,             /* In: Array of digitized points */
        int first,            /* In: Indices defining region */
        int last,             /* In: Indices defining region */
        float* uPrime,        /* In: Parameter values for region */
        Vector2 tHat1,        /* In: Unit tangents at endpoints left */
        Vector2 tHat2,        /* In: Unit tangents at endpoints right */
        BezierCurve* bezCurve /* Out: Cubic bezier curve control points. */
        )
    {
        int     i;
        Vector2 A[MAXPOINTS][2]; /* Precomputed rhs for eqn    */
        int     nPts;            /* Number of pts in sub-curve */
        float   C[2][2];         /* Matrix C  */
        float   X[2];            /* Matrix X  */
        float   det_C0_C1;       /* Determinants of matrices */
        float   det_C0_X;
        float   det_X_C1;
        float   alpha_l;         /* Alpha values, left and right */
        float   alpha_r;
        Vector2 tmp;             /* Utility variable */
        float  segLength;
        float  epsilon;

        nPts = last - first + 1;


        /* Compute the A's    */
        for (i = 0; i < nPts; i++)
        {
            Vector2 v1, v2;
            v1 = tHat1;
            v2 = tHat2;
            V2Scale(&v1, B1(uPrime[i]));
            V2Scale(&v2, B2(uPrime[i]));
            A[i][0] = v1;
            A[i][1] = v2;
        }

        /* Create the C and X matrices    */
        C[0][0] = 0.0;
        C[0][1] = 0.0;
        C[1][0] = 0.0;
        C[1][1] = 0.0;
        X[0] = 0.0;
        X[1] = 0.0;

        for (i = 0; i < nPts; i++)
        {
            C[0][0] += V2Dot(&A[i][0], &A[i][0]);
            C[0][1] += V2Dot(&A[i][0], &A[i][1]);
            /* C[1][0] += V2Dot(&A[i][0], &A[i][1]); */
            C[1][0] = C[0][1];
            C[1][1] += V2Dot(&A[i][1], &A[i][1]);

            tmp = V2SubII(d[first + i],
                V2AddII(
                    V2ScaleIII(d[first], B0(uPrime[i])),
                    V2AddII(
                        V2ScaleIII(d[first], B1(uPrime[i])),
                        V2AddII(
                            V2ScaleIII(d[last], B2(uPrime[i])),
                            V2ScaleIII(d[last], B3(uPrime[i]))))));


            X[0] += V2Dot(&A[i][0], &tmp);
            X[1] += V2Dot(&A[i][1], &tmp);
        }

        /* Compute the determinants of C and X    */
        det_C0_C1 = C[0][0] * C[1][1] - C[1][0] * C[0][1];
        det_C0_X = C[0][0] * X[1] - C[1][0] * X[0];
        det_X_C1 = X[0] * C[1][1] - X[1] * C[0][1];

        /* Finally, derive alpha values    */
        alpha_l = (det_C0_C1 == 0.0f) ? 0.0f : det_X_C1 / det_C0_C1;
        alpha_r = (det_C0_C1 == 0.0f) ? 0.0f : det_C0_X / det_C0_C1;

        /* If alpha negative, use the Wu/Barsky heuristic (see text) */
        /* (if alpha is 0, you get coincident control points that lead to
         * divide by zero in any subsequent newton_raphson_root_find() call. */
        segLength = V2DistanceBetween2Points(&d[last], &d[first]);
        epsilon = 1.0e-6f * segLength;
        if (alpha_l < epsilon || alpha_r < epsilon)
        {
            /* fall back on standard (probably inaccurate) formula, and subdivide further if needed. */
            float dist = segLength / 3.0f;
            bezCurve->p[0] = d[first];
            bezCurve->p[3] = d[last];
            V2Add(&bezCurve->p[0], V2Scale(&tHat1, dist), &bezCurve->p[1]);
            V2Add(&bezCurve->p[3], V2Scale(&tHat2, dist), &bezCurve->p[2]);
        }

        /*  First and last control points of the Bezier curve are */
        /*  positioned exactly at the first and last data points */
        /*  Control points 1 and 2 are positioned an alpha distance out */
        /*  on the tangent vectors, left and right, respectively */
        bezCurve->p[0] = d[first];
        bezCurve->p[3] = d[last];
        V2Add(&bezCurve->p[0], V2Scale(&tHat1, alpha_l), &bezCurve->p[1]);
        V2Add(&bezCurve->p[3], V2Scale(&tHat2, alpha_r), &bezCurve->p[2]);
    }


     /*
      *  newton_raphson_root_find :
      *    Use Newton-Raphson iteration to find better root.
      */
    static float newton_raphson_root_find(
        BezierPointsPtr Q, /*  Current fitted curve */
        point P,           /*  Digitized point  */
        float u           /*  Parameter value for "P" */
    )
    {
        float numerator;
        float denominator;
        point Q1[3]; /*  Q'  */
        point Q2[2]; /*  Q'' */
        /* u evaluated at Q, Q', & Q''    */
        point Q_u;
        point Q1_u;
        point Q2_u;

         /* Compute Q(u)    */
         Q_u = bezier_eval(3, Q, u);

         /* Generate control vertices for Q'    */
         for (int i = 0; i <= 2; i++) {
             Q1[i].x = (Q[i + 1].x - Q[i].x) * 3.0f;
             Q1[i].y = (Q[i + 1].y - Q[i].y) * 3.0f;
         }

         /* Generate control vertices for Q'' */
         for (int i = 0; i <= 1; i++) {
             Q2[i].x = (Q1[i + 1].x - Q1[i].x) * 2.0f;
             Q2[i].y = (Q1[i + 1].y - Q1[i].y) * 2.0f;
         }

         /* Compute Q'(u) and Q''(u)    */
         Q1_u = bezier_eval(2, Q1, u);
         Q2_u = bezier_eval(1, Q2, u);

         /* Compute f(u)/f'(u) */
         numerator = (Q_u.x - P.x) * (Q1_u.x) + (Q_u.y - P.y) * (Q1_u.y);
         denominator = (Q1_u.x) * (Q1_u.x) + (Q1_u.y) * (Q1_u.y) +
             (Q_u.x - P.x) * (Q2_u.x) + (Q_u.y - P.y) * (Q2_u.y);

         if (denominator == 0.0f)
         {
             return u;
         }

         /*  Improved u */
         float uPrime;
         /* u = u - f(u)/f'(u) */
         uPrime = u - (numerator / denominator);
         return uPrime;
    }

     /*
      *  Bezier :
      *      Evaluate a Bezier curve at a particular parameter value
      *
      */
     static point bezier_eval(
         int     degree, /* The degree of the bezier curve     */
         point* V,       /* Array of control points            */
         float  t        /* Parametric value to find point for */
     )
     {
         point pt[4]; /* Local copy of control points  */

         TD_ASSERT(degree <= 3);

         for (int i = 0; i <= degree; i++)
         {
             pt[i] = V[i];
         }

         /* Triangle computation    */
         for (int i = 1; i <= degree; i++) {
             for (int j = 0; j <= degree - i; j++) {
                 pt[j].x = (1.0f - t) * pt[j].x + t * pt[j + 1].x;
                 pt[j].y = (1.0f - t) * pt[j].y + t * pt[j + 1].y;
             }
         }
         return pt[0];
     }

     /*
      *  B0, B1, B2, B3 :
      *    Bezier multipliers
      */
     static float B0(float    u)
     {
         float tmp = 1.0f - u;
         return (tmp * tmp * tmp);
     }

     static float B1(float u)
     {
         float tmp = 1.0f - u;
         return (3.0f * u * (tmp * tmp));
     }

     static float B2(float u)
     {
         float tmp = 1.0f - u;
         return (3.0f * u * u * tmp);
     }

     static float B3(float u)
     {
         return (u * u * u);
     }

     /*
      * ComputeLeftTangent, ComputeRightTangent, ComputeCenterTangent :
      * Approximate unit tangents at endpoints and "center" of digitized curve
      */
     static Vector2 ComputeLeftTangent(
        point* d, /*  Digitized points*/
        int end   /*  Index to "left" end of region */
        )
     {
         Vector2 tHat1 = V2SubII(d[end + 1], d[end]);
         V2Normalize(&tHat1);
         return tHat1;
     }

     static Vector2 ComputeRightTangent(
        point* d,  /*  Digitized points        */
        int end    /*  Index to "right" end of region */
     )
     {
         Vector2 tHat2 = V2SubII(d[end - 1], d[end]);
         V2Normalize(&tHat2);
         return tHat2;
     }

     static Vector2 ComputeCenterTangent(
        point* d,  /*  Digitized points */
        int center  /*  Index to point inside region */
     )
     {
         Vector2    V1, V2, tHatCenter;

         V1 = V2SubII(d[center - 1], d[center]);
         V2 = V2SubII(d[center], d[center + 1]);
         tHatCenter.x = (V1.x + V2.x) / 2.0f;
         tHatCenter.y = (V1.y + V2.y) / 2.0f;
         V2Normalize(&tHatCenter);
         return tHatCenter;
     }

     /*
      *  populate_chord_length_param :
      *    Assign parameter values to digitized points
      *    using relative distances between points.
      * NOTE: compute sum array of length of polylines defined by the points.
      *       The domain of the values is [0.0;1.0]
      */
     static void populate_chord_length_param(
        point* d,  /* Array of digitized points */
        int first,  /*  Indices defining region  */
        int last,   /*  Indices defining region  */
        float* u)  /*  Indices defining region  */
     {
         int        i;

         u[0] = 0.0;
         for (i = first + 1; i <= last; i++)
         {
             u[i - first] = u[i - first - 1] +
                 V2DistanceBetween2Points(&d[i], &d[i - 1]);
         }

         for (i = first + 1; i <= last; i++)
         {
             u[i - first] = u[i - first] / u[last - first];
         }
     }

     /*
      *  compute_max_error :
      *    Find the maximum squared distance of digitized points
      *    to fitted curve.
     */
     static float compute_max_error(
         point* d,                 /*  Array of digitized points  */
         int first,                /*  Indices defining region    */
         int last,                 /*  Indices defining region    */
         BezierPointsPtr bezCurve, /*  Fitted Bezier curve        */
         float* u,                /*  Parameterization of points */
         int* splitPoint           /*  Point of maximum error     */
     )
     {
         int     i;
         float  maxDist; /*  Maximum error  */
         float  dist;    /*  Current error  */
         point  P;        /*  Point on curve */
         Vector2 v;       /*  Vector from point to curve */

         *splitPoint = (last - first + 1) / 2;
         maxDist = 0.0;
         for (i = first + 1; i < last; i++)
         {
             P = bezier_eval(3, bezCurve, u[i - first]);
             v = V2SubII(P, d[i]);
             dist = V2SquaredLength(&v);
             if (dist >= maxDist) {
                 maxDist = dist;
                 *splitPoint = i;
             }
         }
         return (maxDist);
     }
};


static Vector2 V2AddII(Vector2 a, Vector2 b)
{
    Vector2 r;
    r.x = a.x + b.x;
    r.y = a.y + b.y;
    return r;
}

static Vector2 V2ScaleIII(Vector2 v, float s)
{
    Vector2 r;
    r.x = v.x * s;
    r.y = v.y * s;
    return r;
}

static Vector2 V2SubII(Vector2 a, Vector2 b)
{
    Vector2 r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    return r;
}

/* returns squared length of input vector */
static float V2SquaredLength(Vector2* a)
{
    return((a->x * a->x) + (a->y * a->y));
}

/* returns length of input vector */
static float V2Length(Vector2* a)
{
    return(sqrt(V2SquaredLength(a)));
}

/* negates the input vector and returns it */
static Vector2* V2Negate(Vector2* v)
{
    v->x = -v->x;
    v->y = -v->y;
    return v;
}

/* normalizes the input vector and returns it */
static Vector2* V2Normalize(Vector2* v)
{
    float len = V2Length(v);
    if (len != 0.0)
    {
        v->x /= len;
        v->y /= len;
    }
    return v;
}


/* scales the input vector to the new length and returns it */
static Vector2* V2Scale(Vector2* v, float newlen)
{
    float len = V2Length(v);
    if (len != 0.0)
    {
        v->x *= newlen / len;
        v->y *= newlen / len;
    }
    return(v);
}

/* return vector sum c = a+b */
static Vector2* V2Add(Vector2* a, Vector2* b, Vector2* c)
{
    c->x = a->x + b->x;
    c->y = a->y + b->y;
    return(c);
}

/* return the dot product of vectors a and b */
static float V2Dot(Vector2* a, Vector2* b)
{
    return((a->x * b->x) + (a->y * b->y));
}

/* return the distance between two points */
static float V2DistanceBetween2Points(Point2* a, Point2*b)
{
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    return(sqrt((dx * dx) + (dy * dy)));
}

}

#ifdef FIT_CURVES_TESTMODE

/*
 *  main:
 *    Example of how to use the curve-fitting code. Given an array
 *    of points and a tolerance (squared error between points and
 *    fitted curve), the algorithm will generate a piecewise
 *    cubic Bezier representation that approximates the points.
 *    When a cubic is generated, the routine "DrawBezierCurve"
 *    is called, which outputs the Bezier curve just created
 *    (arguments are the degree and the control points, respectively).
 *    Users will have to implement this function themselves
 *    ascii output, etc.
 */
int main()
{
    static point d[7] = {    /*  Digitized points */
        { 0.0, 0.0 },
        { 0.0, 0.5 },
        { 1.1, 1.4 },
        { 2.1, 1.6 },
        { 3.2, 1.1 },
        { 4.0, 0.2 },
        { 4.0, 0.0 },
    };
    float    error = 4.0;  /*  Squared error */

    fit_curve_op op;
    op.fit_curve(d, 7, error); /*  Fit the Bezier curves */
}
#endif                         /* TESTMODE */


#endif // TD_FIT_CURVES_HPP
