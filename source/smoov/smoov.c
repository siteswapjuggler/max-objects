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

#ifdef WIN_VERSION
#define MAXAPI_USE_MSCRT
#endif

#include "ext.h"			// you must include this - it contains the external object's link to available Max functions
#include "ext_obex.h"		// this is required for all objects using the newer style for writing objects.

typedef struct _smoov {         // defines our object's internal variables for each instance in a patch
    t_object    s_ob;			// object header - ALL objects MUST begin with this...
    long        s_len;          // length of the computed list
    char        s_force_output; // force float output or not
    char        s_active;       // is the smoov filter active or not
    double      s_smooth;		// float value - smoothing factor
    t_atom*     s_value0;       // array of last received values
    t_atom*     s_value1;       // array of previous received values
    void*       s_outlet;       // outlet creation - inlets are automatic, but objects must "own" their own outlets
} t_smoov;


// these are prototypes for the methods that are defined below
void *smoov_new(t_symbol *s, long argc, t_atom *argv);
void smoov_free(t_smoov *x);

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

	c = class_new("smoov", (method)smoov_new, (method)smoov_free, sizeof(t_smoov), 0L, A_GIMME, 0); // class_new() loads our external's class into Max's memory so it can be used in a patch

	class_addmethod(c, (method)smoov_bang,		"bang",		0);                 // the method it uses when it gets a bang in the left inlet
    class_addmethod(c, (method)smoov_set,		"set",		A_GIMME,    0);     // the method to set and int or a float in the left inlet       (inlet 0)
	class_addmethod(c, (method)smoov_int,		"int",		A_LONG,     0);     // the method for an int in the left inlet                      (inlet 0)
    class_addmethod(c, (method)smoov_list,		"list",		A_GIMME,     0);    // the method for a list in the left inlet                      (inlet 0)
    class_addmethod(c, (method)smoov_float,		"float",	A_FLOAT,    0);     // the method for a float in the left inlet                     (inlet 0)
	class_addmethod(c, (method)smoov_assist,	"assist",	A_CANT,     0);     // (optional) assistance method needs to be declared like this

    CLASS_ATTR_CHAR(c, "active", 0, t_smoov, s_active);
    CLASS_ATTR_ORDER(c, "active", 0, "1");
    CLASS_ATTR_STYLE_LABEL(c, "active", 0, "onoff", "Toggle smoothing");

    CLASS_ATTR_DOUBLE(c, "smooth", 0, t_smoov, s_smooth);
    CLASS_ATTR_LABEL(c, "smooth", 0, "Smoothing value");
    CLASS_ATTR_ORDER(c, "smooth", 0, "2");
    CLASS_ATTR_FILTER_CLIP(c, "smooth", 0., 1.);
    
    CLASS_ATTR_CHAR(c, "force_output", 0, t_smoov, s_force_output);
    CLASS_ATTR_ORDER(c, "force_output", 0, "3");
    CLASS_ATTR_ENUMINDEX3(c, "force_output",0,"as input","int output","float output");
    CLASS_ATTR_LABEL(c, "force_output", 0, "Fore output style");
    
	class_register(CLASS_BOX, c);
	smoov_class = c;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------

void *smoov_new(t_symbol *s, long argc, t_atom *argv)	// n = float argument typed into object box
{
	t_smoov *x = (t_smoov *)object_alloc(smoov_class); // create a new instance of this object

    x->s_len = 1;                           // set 1 by default
    x->s_active = 1;                        // set active by default
    x->s_force_output = 0;                  // set not active by default
    x->s_smooth = 0.1;                      // set the default value if no or bad argument
    x->s_outlet = outlet_new(x, NULL);      // create a flexible outlet and assign it to our outlet variable in the instance's data structure
    
    if (argc>0) {
        double f;
        switch (atom_gettype(argv)) {
            case A_FLOAT:
                f = atom_getfloat(argv);
                x->s_smooth = (f>1.) ? 1. : (f<0.) ? 0. : f;              // set the initial smooth value
                break;
            case A_SYM:
                if ((atom_getsym(argv)->s_name)[0]=='@') break;           // test for attributes
            default:
                error("smooth: first argument must be a float value.");   // prompt an error message in the max window
                break;
        }
    }
    
    attr_args_process(x, argc, argv);       // process arguments
    
    t_atom *buffer0 = malloc(256*sizeof(t_atom));
    t_atom *buffer1 = malloc(256*sizeof(t_atom));

    x->s_value0 = buffer0;
    x->s_value1 = buffer1;
    
    unsigned short i;
    for (i=0;i<256;i++) {
        atom_setlong(x->s_value0+i,0);     // set initial value in the instance's data structure
        atom_setlong(x->s_value1+i,0);     // set initial value in the instance's data structure
        }
    
	return(x);                              // return a reference to the object instance
}

void smoov_free(t_smoov *x) {
    free(x->s_value0);
    free(x->s_value1);
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
    unsigned short i;
    for (i=0;i<x->s_len;i++) {
        if ((((x->s_value0+i)->a_type == A_LONG) &&(x->s_force_output==0))||(x->s_force_output==1)) {
            double val = (x->s_active) ? atom_getfloat(x->s_value0+i)*x->s_smooth + atom_getfloat(x->s_value1+i)*(1.-x->s_smooth) : atom_getfloat(x->s_value0+i);
            atom_setlong(x->s_value1+i,round(val));
        }
        if ((((x->s_value0+i)->a_type == A_FLOAT)&&(x->s_force_output==0))||(x->s_force_output==2)) {
            double val = (x->s_active) ? atom_getfloat(x->s_value0+i)*x->s_smooth + atom_getfloat(x->s_value1+i)*(1.-x->s_smooth) : atom_getfloat(x->s_value0+i);
            atom_setfloat(x->s_value1+i,val);
        }
    }
    outlet_list(x->s_outlet, NULL, x->s_len,x->s_value1);
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

void smoov_list(t_smoov *x, t_symbol *s, long argc, t_atom *argv)
{
    smoov_set(x,NULL,argc,argv);
    smoov_bang(x);
}


void smoov_set(t_smoov *x, t_symbol *s, long argc, t_atom *argv)
{
    unsigned short i;
    x->s_len = (argc>256) ? 256 : argc;
    for (i=0;i<x->s_len;i++) {
        switch (atom_gettype(argv+i)) {
            case A_LONG:
            case A_FLOAT:
                *(x->s_value0+i) = *(argv+i);
                break;
            default:
                atom_setlong(x->s_value0+i,0    );
                break;
        }
    }
}