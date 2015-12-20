/**
	smoov.c - provide smoothing of data streams

	this object has one inlet and one outlet
	it responds to ints, floats and 'bang' message in the left inlet
	it responds to the 'assistance' message sent by Max when the mouse is positioned over an inlet or outlet
	it smooths values by a factor choosen smooth value
*/

/** 
    TODO-LIST
    optimiser les accès mémoires en changeant de structure de données pour le stockage des listes : Index, AtomArray ???
*/


#include "ext.h"			// you must include this - it contains the external object's link to available Max functions
#include "ext_obex.h"		// this is required for all objects using the newer style for writing objects.

typedef struct _smoov {     // defines our object's internal variables for each instance in a patch
    t_object p_ob;			// object header - ALL objects MUST begin with this...
    long   p_len;           // length of the computed list
    char   p_float_output;  // is the smoov filter active or not
    char   p_active;        // is the smoov filter active or not
    double p_smooth;		// float value - smoothing factor
    t_atom p_value0[256];   // array of last received values
    t_atom p_value1[256];   // array of previous received values
    void *p_outlet;			// outlet creation - inlets are automatic, but objects must "own" their own outlets
} t_smoov;


// these are prototypes for the methods that are defined below
void *smoov_new(t_symbol *s, long argc, t_atom *argv);

void smoov_bang(t_smoov *x);
void smoov_int(t_smoov *x, long n);
void smoov_float(t_smoov *x, double f);
void smoov_list(t_smoov *x, t_symbol *s, long argc, t_atom *argv);
void smoov_set(t_smoov *x, t_symbol *s, long argc, t_atom *argv);
void smoov_assist(t_smoov *x, void *b, long m, long a, char *s);


t_class *smoov_class;		// global pointer to the object class - so max can reference the object

//---------------------------------------------------------------------------------------------------------------------------------------------------------

void ext_main(void *r)
{
	t_class *c;

	c = class_new("smoov", (method)smoov_new, (method)NULL, sizeof(t_smoov), 0L, A_GIMME, 0); // class_new() loads our external's class into Max's memory so it can be used in a patch

	class_addmethod(c, (method)smoov_bang,		"bang",		0);                 // the method it uses when it gets a bang in the left inlet
    class_addmethod(c, (method)smoov_set,		"set",		A_GIMME,    0);     // the method to set and int or a float in the left inlet       (inlet 0)
	class_addmethod(c, (method)smoov_int,		"int",		A_LONG,     0);     // the method for an int in the left inlet                      (inlet 0)
    class_addmethod(c, (method)smoov_list,		"list",		A_GIMME,     0);    // the method for an list in the left inlet                     (inlet 0)
    class_addmethod(c, (method)smoov_float,		"float",	A_FLOAT,    0);     // the method for a float in the left inlet                     (inlet 0)
	class_addmethod(c, (method)smoov_assist,	"assist",	A_CANT,     0);     // (optional) assistance method needs to be declared like this

    CLASS_ATTR_DOUBLE(c, "smooth", 0, t_smoov, p_smooth);
    CLASS_ATTR_LABEL(c, "smooth", 0, "Smoothing value");
    CLASS_ATTR_FILTER_CLIP(c, "smooth", 0., 1.);
    
    CLASS_ATTR_CHAR(c, "active", 0, t_smoov, p_active);
    CLASS_ATTR_STYLE_LABEL(c, "active", 0, "onoff", "Toggle smoothing");

    CLASS_ATTR_CHAR(c, "float_output", 0, t_smoov, p_float_output);
    CLASS_ATTR_STYLE_LABEL(c, "float_output", 0, "onoff", "Float output");
    
	class_register(CLASS_BOX, c);
	smoov_class = c;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------

void *smoov_new(t_symbol *s, long argc, t_atom *argv)	// n = float argument typed into object box
{
	t_smoov *x = (t_smoov *)object_alloc(smoov_class); // create a new instance of this object

    long i;
    for (i=0;i<256;i++) {
        atom_setlong(&x->p_value0[i],0);	// set initial value in the instance's data structure
        atom_setlong(&x->p_value1[i],0);    // set initial value in the instance's data structure
        }
    
    x->p_len = 1;                           // set 0 by default
    x->p_active = 1;                        // set active by default
    x->p_float_output = 0;                  // set not active by default
    x->p_smooth = 0.1;                      // set the default value if no or bad argument
    x->p_outlet = outlet_new(x, NULL);      // create a flexible outlet and assign it to our outlet variable in the instance's data structure

    if (argc>0) {
        double f;
        switch (atom_gettype(argv)) {
            case A_FLOAT:
                f = atom_getfloat(argv);
                x->p_smooth = (f>1.) ? 1. : (f<0.) ? 0. : f;              // set the initial smooth value
                break;
            case A_LONG:
                error("smooth: first argument must be a float value.");   // prompt an error message in the max window
                break;
        }
    }
    
    attr_args_process(x, argc, argv);       // process arguments
    
	return(x);                              // return a reference to the object instance
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------

void smoov_assist(t_smoov *x, void *b, long m, long a, char *s) // 4 final arguments are always the same for the assistance method
{
	if (m == ASSIST_OUTLET)
		sprintf(s,"Smoothed value");
	else
        sprintf(s,"Inlet %ld: values and messages", a);
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------

void smoov_bang(t_smoov *x)
{
    long i;
    for (i=0;i<x->p_len;i++) {
        if ((x->p_value0[i].a_type == A_LONG)&&(x->p_float_output==0)) {
            long val = (x->p_active) ? atom_getlong(&x->p_value0[i])*x->p_smooth + atom_getlong(&x->p_value1[i])*(1.-x->p_smooth) : atom_getlong(&x->p_value0[i]);
            atom_setlong(&x->p_value1[i],val);
        }
        if ((x->p_value0[i].a_type == A_FLOAT)||(x->p_float_output!=0)) {
            double val = (x->p_active) ? atom_getfloat(&x->p_value0[i])*x->p_smooth + atom_getfloat(&x->p_value1[i])*(1.-x->p_smooth) : atom_getfloat(&x->p_value0[i]);
            atom_setfloat(&x->p_value1[i],val);
        }
    }
    outlet_list(x->p_outlet, NULL, x->p_len,x->p_value1);
}


void smoov_int(t_smoov *x, long n)
{
    t_atom av;
    atom_setlong(&av,n);
    smoov_set(x,NULL,1,&av);
	smoov_bang(x);
}

void smoov_float(t_smoov *x, double f)
{
    t_atom av;
    atom_setfloat(&av,f);
    smoov_set(x,NULL,1,&av);
    smoov_bang(x);
}

void smoov_list(t_smoov *x, t_symbol *s, long argc, t_atom *argv) {
    smoov_set(x,NULL,argc,argv);
    smoov_bang(x);
}


void smoov_set(t_smoov *x, t_symbol *s, long argc, t_atom *argv)
{
    if (argc>0) {
        long i;
        x->p_len = (argc>255) ? 255 : argc;
        for (i=0;i<x->p_len;i++) {
            switch (atom_gettype(argv+i)) {
                case A_LONG:
                    atom_setlong(&x->p_value0[i],atom_getlong(argv+i));
                    break;
                case A_FLOAT:
                    atom_setfloat(&x->p_value0[i],atom_getfloat(argv+i));
                    break;
            }
        }
    }
}