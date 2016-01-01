/**
	ramp.c - non-linear interpolation object

	this object has two inlet and one outlet
    it responds to (nature of msg)
	it responds to the 'assistance' message sent by Max when the mouse is positioned over an inlet or outlet
    it provide advanced control over interpolation beetween values
 
    the interpolation equation comes from those website:
        http://sole.github.io/tween.js/examples/03_graphs.html
        https://github.com/CreateJS/TweenJS/blob/master/src/tweenjs/Ease.js
 */

/** 
    TODO-LIST
    structure variable >> voir Flexible Array Member
*/

#ifdef WIN_VERSION
#define MAXAPI_USE_MSCRT
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "ext.h"			// you must include this - it contains the external object's link to available Max functions
#include "ext_obex.h"		// this is required for all objects using the newer style for writing objects.

//---------------------------------------------------------------------------------------------------------------------------------------------------------

#define MODE_LIST          \
    X(LINEAR)              \
    X(QUAD_IN)             \
    X(QUAD_OUT)            \
    X(QUAD_INOUT)          \
    X(CUBIC_IN)            \
    X(CUBIC_OUT)           \
    X(CUBIC_INOUT)         \
    X(QUARTIC_IN)          \
    X(QUARTIC_OUT)         \
    X(QUARTIC_INOUT)       \
    X(QUINTIC_IN)          \
    X(QUINTIC_OUT)         \
    X(QUINTIC_INOUT)       \
    X(SINUSOIDAL_IN)       \
    X(SINUSOIDAL_OUT)      \
    X(SINUSOIDAL_INOUT)    \
    X(EXPONENTIAL_IN)      \
    X(EXPONENTIAL_OUT)     \
    X(EXPONENTIAL_INOUT)   \
    X(CIRCULAR_IN)         \
    X(CIRCULAR_OUT)        \
    X(CIRCULAR_INOUT)      \
    X(ELASTIC_IN)          \
    X(ELASTIC_OUT)         \
    X(ELASTIC_INOUT)       \
    X(BACK_IN)             \
    X(BACK_OUT)            \
    X(BACK_INOUT)          \
    X(BOUNCE_IN)           \
    X(BOUNCE_OUT)          \
    X(BOUNCE_INOUT)

enum mode {
#define X(name) name,
    MODE_LIST
#undef X
    LAST
};

char *name[] = {
#define X(name) #name,
    MODE_LIST
#undef X
};

typedef struct _inter {
    double      bgn;        // beginning of the ramp
    double      dst;        // end of the ramp
    double      act;        // actual value
    long        prog;       // progression of the ramp
    long        time;       // length of the ramp (in ms)
    bool        mask;       // is this element masked or not
    enum mode   mode;       // type of interpolation: linear, etc...
    e_max_atomtypes type;   // type of data A_FLOAT or A_LONG
} t_inter;

//---------------------------------------------------------------------------------------------------------------------------------------------------------

typedef struct _ramp {          // defines our object's internal variables for each instance in a patch
    t_object    r_ob;			// object header - ALL objects MUST begin with this...
    long        r_in;           // store inlet number
    long        r_len;          // length of the computed list
    double      r_time;         // last clock time
    double      r_resume;       // time to resume when pause
    long        r_grain;        // interval beetween outputs
    char        r_reset_time;   // reset time to 0 when a ramp is done
    char        r_force_output; // force data output type
    t_inter     *r_values;      // array of ramped values
    void        *r_clock;       // set a clock for this object
    void        *r_proxy;       // inlet proxy
    void        *r_outlet1;		// outlet creation - inlets are automatic, but objects must "own" their own outlets
    void        *r_outlet2;		// outlet creation - inlets are automatic, but objects must "own" their own outlets
    void        *r_outlet3;		// outlet creation - inlets are automatic, but objects must "own" their own outlets
} t_ramp;

void *ramp_new(t_symbol *s, long argc, t_atom *argv);
void ramp_free(t_ramp *x);

void ramp_int(t_ramp *x, long n);
void ramp_float(t_ramp *x, double f);
void ramp_list(t_ramp *x, t_symbol *s, long argc, t_atom *argv);
void ramp_set(t_ramp *x, t_symbol *s, long argc, t_atom *argv);
void ramp_time(t_ramp *x, t_symbol *s, long argc, t_atom *argv);
void ramp_mode(t_ramp *x, t_symbol *s, long argc, t_atom *argv);
void ramp_mask(t_ramp *x, t_symbol *s, long argc, t_atom *argv);
void ramp_any(t_ramp *x, t_symbol *s, long argc, t_atom *argv);

void ramp_stop(t_ramp *x);
void ramp_pause(t_ramp *x);
void ramp_resume(t_ramp *x);
void ramp_update(t_ramp *x);

double powin(double k, long p);
double powout(double k, long p);
double powinout(double k, long p);
double ramp_calc(double k, enum mode m);

void ramp_assist(t_ramp *x, void *b, long m, long a, char *s);


t_class *ramp_class;		// global pointer to the object class - so max can reference the object

//---------------------------------------------------------------------------------------------------------------------------------------------------------

void ext_main(void *r) {

    t_class *c;
    c = class_new("ramp", (method)ramp_new, (method)ramp_free, sizeof(t_ramp), 0L, A_GIMME, 0); // class_new() loads our external's class into Max's memory so it can be used in a patch

	class_addmethod(c, (method)ramp_int,		"int",		A_LONG,     0);     // the method for an int in the left inlet                          (inlet 0)
    class_addmethod(c, (method)ramp_float,		"float",	A_FLOAT,    0);     // the method for a float in the left inlet                         (inlet 0)
    class_addmethod(c, (method)ramp_list,		"list",		A_GIMME,    0);     // the method for a list in the left inlet                          (inlet 0)
    class_addmethod(c, (method)ramp_time,		"time",		A_GIMME,    0);     // the method to set an int, a float or a list in the time inlet    (inlet 0)
    class_addmethod(c, (method)ramp_mode,		"mode",		A_GIMME,    0);     // the method to set an int, a float or a list in the kind inlet    (inlet 0)
    class_addmethod(c, (method)ramp_mask,		"mask",		A_GIMME,    0);     // the method to set an int, a float or a list in the kind inlet    (inlet 0)
    
    class_addmethod(c, (method)ramp_stop,		"stop",     NULL,       0);     // stop the current ramp                                            (inlet 0)
    class_addmethod(c, (method)ramp_pause,		"pause",	NULL,       0);     // pause the current ramp                                           (inlet 0)
    class_addmethod(c, (method)ramp_resume,		"resume",	NULL,       0);     // resume the current ramp                                          (inlet 0)
    
    class_addmethod(c, (method)ramp_any,        "anything", A_GIMME,    0);
    class_addmethod(c, (method)ramp_assist,     "assist",	A_CANT,     0);     // (optional) assistance method needs to be declared like this
    class_addmethod(c, (method)stdinletinfo,    "inletinfo",A_CANT,     0);     // (optional) get all left inlet cold

    CLASS_ATTR_LONG(c, "grain", 0, t_ramp, r_grain);
    CLASS_ATTR_FILTER_MIN(c, "grain", 1);
    CLASS_ATTR_ORDER(c, "grain", 0, "1");
    CLASS_ATTR_LABEL(c, "grain", 0, "Grain in Milliseconds");

    CLASS_ATTR_CHAR(c, "reset_time", 0, t_ramp, r_reset_time);
    CLASS_ATTR_ORDER(c, "reset_time", 0, "2");
    CLASS_ATTR_STYLE_LABEL(c, "reset_time", 0, "onoff", "Reset time when finished");
    
    CLASS_ATTR_CHAR(c, "force_output", 0, t_ramp, r_force_output);
    CLASS_ATTR_ORDER(c, "force_output", 0, "3");
    CLASS_ATTR_ENUMINDEX3(c, "force_output",0,"as input","int output","float output");
    CLASS_ATTR_LABEL(c, "force_output", 0, "Fore output style");

    
	class_register(CLASS_BOX, c);
	ramp_class = c;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------

void *ramp_new(t_symbol *s, long argc, t_atom *argv) {	// n = float argument typed into object box
    
	t_ramp *x = (t_ramp *)object_alloc(ramp_class);     // create a new instance of this object
    
    t_inter *buffer = malloc(256*sizeof(t_inter));
    x->r_values = buffer;
    
    float val   = 0.;
    long  time  = 0.;
    long  grain = 20;
    long  mode  = LINEAR;
    
    unsigned char  argorder = 0;
    
    unsigned short i,j;
    for (i=0;(i<argc)&&(i<2);i++) {
        
        /* arguments:
            1. (long/sym) time or mode value
            2. (long/sym) time or mode value
         */
        
        switch (atom_gettype(argv+i)) {
            case A_LONG:
                if (i==1&&argorder==0) {
                    post("ramp: time can only be set once as an argument");
                    break;
                }
                time = atom_getlong(argv+i);
                if (time < 0) time = 0;
                else break;
                post("ramp: time cannot be a negative value, value has been set to %d",time);
                break;

            case A_FLOAT:
                if (i==1&&argorder==0) {
                    post("ramp: time can only be set once as an argument");
                    break;
                }
                time = round(atom_getfloat(argv+i));
                post("ramp: time should be an int, value has been rounded");
                if (time < 0) time = 0;
                else break;
                post("ramp: time cannot be a negative value, value has been set to %d",time);
                break;
                
            case A_SYM:
                if ((atom_getsym(argv+i)->s_name)[0]=='@')
                    goto settings;

                if (i==1&&argorder==1) {
                    post("ramp: mode can only be set once as an argument");
                    break;
                }
                
                for (j=0;j<LAST;j++) {
                    if (strcmp(atom_getsym(argv+i)->s_name,name[j])==0) {
                        mode = j;
                        break;
                    }
                }
                if (mode == j) {
                    if (i==0) argorder = 1;
                    break;
                }

            default:
                switch(i) {
                    case 0:
                    case 1:
                        error("ramp: argument %d must be interpolation mode or time value (0-inf)",i+1);
                        break;
                }
        }
    }

    settings:
    for (i=0;i<256;i++) {
        (x->r_values+i)->bgn   = val;         // set initial value in the instance's data structure
        (x->r_values+i)->dst   = val;         // set initial value in the instance's data structure
        (x->r_values+i)->act   = val;         // set initial value in the instance's data structure
        (x->r_values+i)->prog  = val;         // set initial value in the instance's data structure
        (x->r_values+i)->time  = time;        // set initial value in the instance's data structure
        (x->r_values+i)->mode  = mode;        // set initial value in the instance's data structure
        (x->r_values+i)->mask  = true;           // set initial value in the instance's data structure
        (x->r_values+i)->type  = A_LONG;      // set initial value in the instance's data structure
        }
    
    x->r_len = 1;                           // set 1 by default
    x->r_grain = grain;                     // set 20 ms grain by default
    x->r_reset_time = 0;                    // set not active by default
    x->r_force_output = 0;                  // set not active by default
    
    attr_args_process(x, argc, argv);       // process arguments

    x->r_outlet3 = outlet_new(x, NULL);                          // create a flexible outlet and assign it to our outlet variable in the instance's data structure
    x->r_outlet2 = outlet_new(x, NULL);                          // create a flexible outlet and assign it to our outlet variable in the instance's data structure
    x->r_outlet1 = outlet_new(x, NULL);                          // create a flexible outlet and assign it to our outlet variable in the instance's data structure

    x->r_proxy = proxy_new((t_object *)x, 3, &x->r_in);          // create grain inlet
    x->r_proxy = proxy_new((t_object *)x, 2, &x->r_in);          // create mode inlet
    x->r_proxy = proxy_new((t_object *)x, 1, &x->r_in);          // create time inlet
    
    x->r_clock = clock_new((t_object *)x, (method)ramp_update);  // create a clock for the object
    
	return(x);                              // return a reference to the object instance
}

void ramp_free(t_ramp *x) {
    free(x->r_values);
    freeobject(x->r_clock);
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------

void ramp_assist(t_ramp *x, void *b, long m, long a, char *s) {
    if (m == ASSIST_INLET) {
        switch (a) {
            case 0:
                sprintf(s,"Destination Value of Ramp");
                break;
            case 1:
                sprintf(s,"Total Ramp Time in Milliseconds");
                break;
            case 2:
                sprintf(s,"Ramp Interpolation Mode");
                break;
            case 3:
                sprintf(s,"Time Grain in Milliseconds");
                break;
        }
    }
    else {
        switch (a) {
            case 0:
                sprintf(s,"Ramp Output");
                break;
            case 1:
                sprintf(s,"Signal End of Ramp");
                break;
            case 2:
                sprintf(s,"Dumpout");
                break;
        }
        
    }
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------

void ramp_bang(t_ramp *x) {
    unsigned short i;
    bool noramp = true;
    t_atom *temp = malloc(x->r_len*sizeof(t_atom));
    
    for (i=0;i<x->r_len;i++) {
        // if ramp time = 0, output result directly and bang for finish immediatly if all ramp times == 0
        if  ((x->r_values+i)->time == 0)
            (x->r_values+i)->act = (x->r_values+i)->dst;
        else if  ((x->r_values+i)->act != (x->r_values+i)->dst)
            noramp = false;
        
        // get foat or int value output depending on input and attribute settings
        if ((((x->r_values+i)->type == A_LONG) &&(x->r_force_output == 0))||(x->r_force_output == 1)) atom_setlong(&temp[i],round((x->r_values+i)->act));
        if ((((x->r_values+i)->type == A_FLOAT)&&(x->r_force_output == 0))||(x->r_force_output == 2)) atom_setfloat(&temp[i],(x->r_values+i)->act);
        }
    
    outlet_list(x->r_outlet1, NULL, x->r_len,temp);
    free(temp);
    if (noramp == true) outlet_bang(x->r_outlet2);
}

void ramp_int(t_ramp *x, long n) {
    t_atom av;
    switch (proxy_getinlet((t_object *)x)) {
        case 0:
            atom_setlong(&av,n);
            ramp_set(x,NULL,1,&av);
            ramp_stop(x);
            ramp_bang(x);
            clock_delay(x->r_clock,x->r_grain);
            break;
        case 1:
            atom_setlong(&av,n);
            ramp_time(x,NULL,1,&av);
            break;
        case 2:
            atom_setlong(&av,n);
            ramp_mode(x,NULL,1,&av);
            break;
        case 3:
            x->r_grain = (n>100) ? 100 : (n<1) ? 1 : n;
            break;
    }
}

void ramp_float(t_ramp *x, double f) {
    t_atom av;
    switch (proxy_getinlet((t_object *)x)) {
        case 0:
            atom_setfloat(&av,f);
            ramp_set(x,NULL,1,&av);
            ramp_stop(x);
            ramp_bang(x);
            clock_delay(x->r_clock,x->r_grain);
            break;
        case 1:
            atom_setlong(&av,f);
            ramp_time(x,NULL,1,&av);
            break;
        case 2:
            atom_setlong(&av,f);
            ramp_mode(x,NULL,1,&av);
            break;
        case 3:
            x->r_grain = (long)((f>100) ? 100 : (f<1) ? 1 : f);
            break;
    }
}

void ramp_list(t_ramp *x, t_symbol *s, long argc, t_atom *argv) {
    switch (proxy_getinlet((t_object *)x)) {
        case 0:
            ramp_set(x,NULL,argc,argv);
            ramp_stop(x);
            ramp_bang(x);
            clock_delay(x->r_clock,x->r_grain);
            break;
        case 1:
            ramp_time(x,NULL,argc,argv);
            break;
        case 2:
            ramp_mode(x,NULL,argc,argv);
            break;
        case 3:
            x->r_grain=(long)atom_getlong(argv);
            break;
    }
}

void ramp_any(t_ramp *x, t_symbol *s, long argc, t_atom *argv) {
    unsigned char i;
    t_atom av;
    switch (proxy_getinlet((t_object *)x)) {
        case 0:
            if (strcmp(s->s_name,"unmask")==0) {
                atom_setlong(&av,1);
                ramp_mask(x, NULL, 1, &av);
                break;
            }
        case 2:
            for (i=0;i<LAST;i++) {
                if (strcmp(s->s_name,name[i])==0) {
                    ramp_mode(x,s,argc,argv);
                    break;
                    }
            }
    }
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------

void ramp_set(t_ramp *x, t_symbol *s, long argc, t_atom *argv) {
    if (argc != 0) {
        unsigned short i;
        x->r_len = (argc>256) ? 256 : argc;
        for (i=0;i<256;i++) {
            switch (atom_gettype(argv+(i%argc))) {
                case A_LONG:
                case A_FLOAT:
                    if ((x->r_values+i)->mask == true) {
                        (x->r_values+i)->bgn  = (x->r_values+i)->act;               // new begin is actual value
                        (x->r_values+i)->dst  = atom_getfloat(argv+(i%argc));     // new destination is the transmitted value
                        (x->r_values+i)->prog = 0;                                // new destination mean new start
                        (x->r_values+i)->type = atom_gettype(argv+(i%argc));      // new destination has a type
                    }
                    else if (i>=argc)
                        (x->r_values+i)->bgn =
                        (x->r_values+i)->act =
                        (x->r_values+i)->dst = 0;                                 // for value above list length reset to 0
                    break;
            }
        }
    }
}

void ramp_time(t_ramp *x, t_symbol *s, long argc, t_atom *argv) {
    unsigned short i;
    if (argc != 0) {
        for (i=0;i<256;i++) {
            switch (atom_gettype(argv+(i%argc))) {
                case A_LONG:
                case A_FLOAT:
                    if ((x->r_values+i)->mask == true)
                        (x->r_values+i)->time = atom_getlong(argv+(i%argc));
                    break;
            }
        }
    }
    else {
		t_atom *temp = malloc(x->r_len*sizeof(t_atom));
        for (i=0;i<x->r_len;i++) {
                atom_setlong(&temp[i],(x->r_values+i)->time);
            }
        outlet_anything(x->r_outlet3, gensym("time"), x->r_len, temp);
		free(temp);
    }
}

void ramp_mode(t_ramp *x, t_symbol *s, long argc, t_atom *argv) {
    unsigned short i,j;
    
    if (argc == 0) {
        if (s) {
            for (i=0;i<LAST;i++) {
                if (strcmp(s->s_name,name[i])==0) {
                    for (j=0;j<256;j++) {
                        if ((x->r_values+i)->mask == true)
                            x->r_values[j].mode=i;
                    }
                    return;
                }
            }
            if (strcmp(s->s_name,"mode")==0) {
				t_atom *temp = malloc(x->r_len*sizeof(t_atom));
				for (i = 0; i<x->r_len; i++) {
                    atom_setlong(&temp[i],(x->r_values+i)->mode);
                }
                outlet_anything(x->r_outlet3, gensym("mode"), x->r_len, temp);
				free(temp);
				return;
            }
            else {
                post("ramp: do not understant %s",s->s_name);
                return;
            }
        }
    }
    else {
        unsigned char mode;
        if (s) {
            for (i=0;i<LAST;i++) {
                if (strcmp(s->s_name,name[i])==0) {
                    mode=i;
                    break;
                }
            }
        }
        for (i=0;i<256;i++) {
            if ((s)&&(i%(argc+1) == 0)) {
                if ((x->r_values+i)->mask == true)
                    (x->r_values+i)->mode=mode;
            }
            else {
                switch (atom_gettype(argv+(i%(argc+(s?1:0))-(s?1:0)))) {
                    case A_LONG:
                    case A_FLOAT:
                        if ((x->r_values+i)->mask == true)
                            (x->r_values+i)->mode = atom_getlong(argv+(i%(argc+(s?1:0))-(s?1:0)));
                        break;
                    case A_SYM:
                        for (j=0;j<LAST;j++) {
                            if (strcmp(atom_getsym(argv+(i%(argc+(s?1:0))-(s?1:0)))->s_name,name[j])==0) {
                                if ((x->r_values+i)->mask == true)
                                    (x->r_values+i)->mode = j;
                                break;
                            }
                        }
                        break;
                }
            }
        }
    }
}

void ramp_mask(t_ramp *x, t_symbol *s, long argc, t_atom *argv) {
    unsigned short i;
    if (argc != 0) {
        for (i=0;i<256;i++) {
            switch (atom_gettype(argv+(i%argc))) {
                case A_LONG:
                case A_FLOAT:
                    (x->r_values+i)->mask = atom_getlong(argv+(i%argc)) == 0 ? false : true;
                    break;
            }
        }
    }
    else {
        t_atom *temp = malloc(x->r_len*sizeof(t_atom));
        for (i=0;i<x->r_len;i++) {
            atom_setlong(&temp[i],(x->r_values+i)->mask);
        }
        outlet_anything(x->r_outlet3, gensym("time"), x->r_len, temp);
        free(temp);
    }
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------

void ramp_update(t_ramp *x) {
    unsigned short i;
    bool output = false;
    bool finished = true;
    
    clock_getftime(&x->r_time);
    
    for (i=0;i<x->r_len;i++) {
        double val   = (x->r_values+i)->bgn;
        double dst   = (x->r_values+i)->dst;
        
        if ((dst != (x->r_values+i)->act) && (dst != val)) {
            output = true;
            if (((x->r_values+i)->time - (x->r_values+i)->prog) > x->r_grain ) {
                finished = false;
                (x->r_values+i)->prog += x->r_grain;
                val += (dst-val)*ramp_calc((x->r_values+i)->prog/(double)(x->r_values+i)->time,(x->r_values+i)->mode);
            }
            else {
                (x->r_values+i)->prog = (x->r_values+i)->time;
                val = dst;
            }
            (x->r_values+i)->act=val;
        }
        if ((x->r_values+i)->prog == (x->r_values+i)->time) {
            if (x->r_reset_time==1)
                (x->r_values+i)->prog = (x->r_values+i)->time = 0;
        }
    }

    //-------- output the result
    if (finished == false) clock_delay(x->r_clock,x->r_grain);
    else if (output == true) {
        outlet_bang(x->r_outlet2);
        clock_unset(x->r_clock);
        }
    if (output == true) ramp_bang(x);
}

void ramp_stop(t_ramp *x) {
    //stop the ramp and set actual result to be the actual value
    x->r_resume = 0;
    clock_unset(x->r_clock);
}

void ramp_pause(t_ramp *x) {
    //pause the clock and store the time ellapsed since the last clock, keep the rest for resume
    double t;
    clock_unset(x->r_clock);
    clock_getftime(&t);
    x->r_resume = t-x->r_time;
}

void ramp_resume(t_ramp *x) {
    //resume the clock from last pause
    if (x->r_resume!=0) {
        x->r_resume = 0;
        clock_fdelay(x->r_clock,x->r_grain);
        }
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------

double powin(double k, long p) {
    return pow(k,p);
}

double powout(double k, long p) {
    return 1-pow(1-k,p);
}

double powinout(double k, long p) {
    k *= 2;
    if (k<1)
        return 0.5*pow(k,p);
    return 1-0.5*fabs(pow(2-k,p));
}

double ramp_calc(double k, enum mode m) {
    
    double a, p, s;
    
    switch (m) {
        case QUAD_IN:
            return powin(k,2);
            
        case QUAD_OUT:
            return powout(k,2);
            
        case QUAD_INOUT:
            return powinout(k,2);
            
        case CUBIC_IN:
            return powin(k,3);
            
        case CUBIC_OUT:
            return powout(k,3);
            
        case CUBIC_INOUT:
            return powinout(k,3);
            
        case QUARTIC_IN:
            return powin(k,4);
            
        case QUARTIC_OUT:
            return powout(k,4);
            
        case QUARTIC_INOUT:
            return powinout(k,4);
            
        case QUINTIC_IN:
            return powin(k,5);
            
        case QUINTIC_OUT:
            return powout(k,5);
            
        case QUINTIC_INOUT:
            return powinout(k,5);

        case SINUSOIDAL_IN:
            return 1-cos(k*(M_PI/2));
            
        case SINUSOIDAL_OUT:
            return sin(k*(M_PI/2));
            
        case SINUSOIDAL_INOUT:
            return -0.5*(cos(M_PI*k)-1);
            
        case EXPONENTIAL_IN:
            return pow(2,10*(k-1));
            
        case EXPONENTIAL_OUT:
            return (1-pow(2,-10*k));
            
        case EXPONENTIAL_INOUT:
            k *= 2.;
            if (k<1)
                return 0.5*pow(2,10*(k-1));
            k--;
            return 0.5*(2-pow(2,-10*k));
            
        case CIRCULAR_IN:
            return -(sqrt(1-k*k)-1);
            
        case CIRCULAR_OUT:
            k--;
            return sqrt(1-k*k);
            
        case CIRCULAR_INOUT:
            k *= 2;
            if (k<1)
                return -0.5*(sqrt(1-k*k)-1);
            k -= 2;
            return 0.5*(sqrt(1-k*k)+1);

        case ELASTIC_IN:
            if (k == 0 || k == 1)
                return k;
            k -= 1;
            a = 1;
            p = 0.3*1.5;
            s = p*asin(1/a) / (2*M_PI);
            return -a*pow(2,10*k)*sin((k-s)*(2*M_PI)/p);
            
        case ELASTIC_OUT:       //BUG
            if (k == 0 || k == 1)
                return k;
            a = 1;
            p = 0.3;
            s = p*asin(1/a) / (2*M_PI);
            return (a*pow(2,-10*k)*sin((k-s)*(2*M_PI)/p)+1);

            
        case ELASTIC_INOUT:     //BUG
            if (k == 0 || k == 1)
                return k;
            k = k*2 - 1;
            a = 1;
            p = 0.3*1.5;
            s = p*asin(1/a) / (2*M_PI);
            if ((k + 1) < 1)
                return -0.5*a*pow(2,10*k)*sin((k-s)*(2*M_PI)/p);
            return a*pow(2,-10*k)*sin((k-s)*(2*M_PI)/p)*0.5+1;
            
        case BACK_IN:
            s = 1.70158;
            return k*k*((s+1)*k-s);
            
        case BACK_OUT:
            k--;
            s = 1.70158;
            return k*k*((s+1)*k+s)+1;
            
        case BACK_INOUT:
            k *= 2;
            s = 1.70158;
            s *= 1.525;
            if (k < 1)
                return 0.5*k*k*((s+1)*k-s);
            k -= 2;
            return 0.5*k*k*((s+1)*k+s)+1;

        case BOUNCE_IN:
            return 1-ramp_calc(1-k,BOUNCE_OUT);
            
        case BOUNCE_OUT:
            if (k < (1/2.75))
                return 7.5625*k*k;
            if (k < (2/2.75)) {
                k -= 1.5/2.75;
                return 7.5625*k*k+0.75;
            }
            if (k < (2.5/2.75)) {
                k -= (2.25/2.75);
                return 7.5625*k*k+0.9375;
            }
            k -= (2.625/2.75);
            return 7.5625*k*k+0.984375;
            
        case BOUNCE_INOUT:
            if (k < 0.5) {
                return ramp_calc(k*2,BOUNCE_IN)*0.5;
            }
            return ramp_calc(k*2-1,BOUNCE_OUT)*0.5+0.5;
            break;
            
        case LINEAR:
        default:
            return k;
    }
}