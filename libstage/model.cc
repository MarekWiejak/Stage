/** @defgroup model

The basic model simulates an object with basic properties; position,
size, velocity, color, visibility to various sensors, etc. The basic
model also has a body made up of a list of lines. Internally, the
basic model is used as the base class for all other model types. You
can use the basic model to simulate environmental objects.

API: Stg::StgModel 

<h2>Worldfile properties</h2>

@par Summary and default values

@verbatim
model
(
  pose [ 0.0 0.0 0.0 0.0 ]
  size [ 0.1 0.1 0.1 ]
  origin [ 0.0 0.0 0.0 0.0 ]
  velocity [ 0.0 0.0 0.0 0.0 ]

  color "red"
  color_rgba [ 0.0 0.0 0.0 1.0 ]
  bitmap ""
  ctrl ""

  # determine how the model appears in various sensors
  fiducial_return 0
  fiducial_key 0
  obstacle_return 1
  ranger_return 1
  blob_return 1
  laser_return LaserVisible
  gripper_return 0

  # GUI properties
  gui_nose 0
  gui_grid 0
  gui_outline 1
  gui_movemask <0 if top level or (STG_MOVE_TRANS | STG_MOVE_ROT)>;

  blocks 0
  block[0].points 0
  block[0].point[0] [ 0.0 0.0 ]
  block[0].z [ 0.0 1.0 ]
  block[0].color "<color>"

  boundary 0
  mass 10.0
  map_resolution 0.1
  say ""
  alwayson 0
)
@endverbatim

@par Details
 
- pose [ x:<float> y:<float> z:<float> heading:<float> ] \n
  specify the pose of the model in its parent's coordinate system
- size [ x:<float> y:<float> z:<float> ]\n
  specify the size of the model in each dimension
- origin [ x:<float> y:<float> z:<float> heading:<float> ]\n
  specify the position of the object's center, relative to its pose
- velocity [ x:<float> y:<float> z:<float> heading:<float> omega:<float> ]\n
  Specify the initial velocity of the model. Note that if the model hits an obstacle, its velocity will be set to zero.
 
- color <string>\n
  specify the color of the object using a color name from the X11 database (rgb.txt)
- bitmap filename:<string>\n
  Draw the model by interpreting the lines in a bitmap (bmp, jpeg, gif, png supported). The file is opened and parsed into a set of lines.  The lines are scaled to fit inside the rectangle defined by the model's current size.
- ctrl <string>\n
  specify the controller module for the model
 
- fiducial_return fiducial_id:<int>\n
  if non-zero, this model is detected by fiducialfinder sensors. The value is used as the fiducial ID.
- fiducial_key <int>
  models are only detected by fiducialfinders if the fiducial_key values of model and fiducialfinder match. This allows you to have several independent types of fiducial in the same environment, each type only showing up in fiducialfinders that are "tuned" for it.
- obstacle_return <int>\n
  if 1, this model can collide with other models that have this property set 
- ranger_return <int>\n
  if 1, this model can be detected by ranger sensors
- blob_return <int>\n
  if 1, this model can be detected in the blob_finder (depending on its color)
- laser_return <int>\n
  if 0, this model is not detected by laser sensors. if 1, the model shows up in a laser sensor with normal (0) reflectance. If 2, it shows up with high (1) reflectance.
- gripper_return <int>\n
  iff 1, this model can be gripped by a gripper and can be pushed around by collisions with anything that has a non-zero obstacle_return.
 
- gui_nose <int>\n
  if 1, draw a nose on the model showing its heading (positive X axis)
- gui_grid <int>\n
  if 1, draw a scaling grid over the model
- gui_outline <int>\n
  if 1, draw a bounding box around the model, indicating its size
- gui_movemask <int>\n
  define how the model can be moved by the mouse in the GUI window

*/

/*
  TODO

  - friction float [WARNING: Friction model is not yet implemented;
  - details may change] if > 0 the model can be pushed around by other
  - moving objects. The value determines the proportion of velocity
  - lost per second. For example, 0.1 would mean that the object would
  - lose 10% of its speed due to friction per second. A value of zero
  - (the default) means this model can not be pushed around (infinite
  - friction).
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

//#define DEBUG 0
#include "stage_internal.hh"
#include "texture_manager.hh"
//#include <limits.h> 


//static const members
static const bool DEFAULT_BOUNDARY = false;
static const stg_color_t DEFAULT_COLOR = (0xFFFF0000); // solid red
static const stg_joules_t DEFAULT_ENERGY_CAPACITY = 1000.0;
static const bool DEFAULT_ENERGY_CHARGEENABLE = true;
static const stg_watts_t DEFAULT_ENERGY_GIVERATE =  0.0;
static const stg_meters_t DEFAULT_ENERGY_PROBERANGE = 0.0;
static const stg_watts_t DEFAULT_ENERGY_TRICKLERATE = 0.1;
static const stg_meters_t DEFAULT_GEOM_SIZEX = 0.10;
static const stg_meters_t DEFAULT_GEOM_SIZEY = 0.10;
static const stg_meters_t DEFAULT_GEOM_SIZEZ = 0.10;
static const bool DEFAULT_GRID = false;
static const bool DEFAULT_GRIPPERRETURN = false;
static const stg_laser_return_t DEFAULT_LASERRETURN = LaserVisible;
static const stg_meters_t DEFAULT_MAP_RESOLUTION = 0.1;
static const stg_movemask_t DEFAULT_MASK = (STG_MOVE_TRANS | STG_MOVE_ROT);
static const stg_kg_t DEFAULT_MASS = 10.0;
static const bool DEFAULT_NOSE = false;
static const bool DEFAULT_OBSTACLERETURN = true;
static const bool DEFAULT_BLOBRETURN = true;
static const bool DEFAULT_OUTLINE = true;
static const bool DEFAULT_RANGERRETURN = true;

// speech bubble colors
static const stg_color_t BUBBLE_FILL = 0xFFC8C8FF; // light blue/grey
static const stg_color_t BUBBLE_BORDER = 0xFF000000; // black
static const stg_color_t BUBBLE_TEXT = 0xFF000000; // black

// static members
uint32_t StgModel::count = 0;
GHashTable* StgModel::modelsbyid = g_hash_table_new( NULL, NULL );


// constructor
StgModel::StgModel( StgWorld* world,
						  StgModel* parent,
						  const stg_model_type_t type )
  : StgAncestor()
{
  assert( modelsbyid );
  assert( world );
  
  PRINT_DEBUG3( "Constructing model world: %s parent: %s type: %d ",
					 world->Token(), 
					 parent ? parent->Token() : "(null)",
					 type );
  
  this->parent = parent;
  this->world = world;  
  this->type = type;
  this->id = StgModel::count++; // assign a unique ID and increment
  // the global model counter
  
  g_hash_table_insert( modelsbyid, (void*)this->id, this );

  // Adding this model to its ancestor also gives this model a
  // sensible default name
  if ( parent ) 
    parent->AddChild( this );
  else
    world->AddChild( this );
	
  world->AddModel( this );
  
  bzero( &pose, sizeof(pose));

  bzero( &global_pose, sizeof(global_pose));
  this->gpose_dirty = true;
  
  this->trail = g_array_new( false, false, sizeof(stg_trail_item_t) );

  this->data_fresh = false;
  this->disabled = false;
  this->blocks = NULL;
  this->rebuild_displaylist = true;
  this->say_string = NULL;
  this->subs = 0;
  this->stall = false;

  if( world->IsGUI() ) 
    this->blocks_dl = glGenLists( 1 );

  this->geom.size.x = DEFAULT_GEOM_SIZEX;
  this->geom.size.y = DEFAULT_GEOM_SIZEY;
  this->geom.size.z = DEFAULT_GEOM_SIZEZ;
  memset( &this->geom.pose, 0, sizeof(this->geom.pose));

  this->obstacle_return = DEFAULT_OBSTACLERETURN;
  this->ranger_return = DEFAULT_RANGERRETURN;
  this->blob_return = DEFAULT_BLOBRETURN;
  this->laser_return = DEFAULT_LASERRETURN;
  this->gripper_return = DEFAULT_GRIPPERRETURN;
  this->fiducial_return = 0;
  this->fiducial_key = 0;

  this->boundary = DEFAULT_BOUNDARY;
  this->color = DEFAULT_COLOR;
  this->map_resolution = DEFAULT_MAP_RESOLUTION; // meters

  this->gui_nose = DEFAULT_NOSE;
  this->gui_grid = DEFAULT_GRID;
  this->gui_outline = DEFAULT_OUTLINE;
  this->gui_mask = this->parent ? 0 : DEFAULT_MASK;

  this->callbacks = g_hash_table_new( g_int_hash, g_int_equal );
  this->flag_list = NULL;
  this->blinkenlights = g_ptr_array_new();

  bzero( &this->velocity, sizeof(velocity));

  this->on_velocity_list = false;

  this->last_update = 0;
  this->interval = (stg_usec_t)1e4; // 10msec

  this->initfunc = NULL;

  this->wf = NULL;
  this->wf_entity = 0;

  // now we can add the basic square shape
  this->AddBlockRect( -0.5,-0.5,1,1 );
		
  PRINT_DEBUG2( "finished model %s @ %p", 
					 this->token, this );
}

StgModel::~StgModel( void )
{
  UnMap(); // remove from the raytrace bitmap

  // children are removed in ancestor class
  
  // remove from parent, if there is one
  if( parent ) 
    parent->children = g_list_remove( parent->children, this );
  else
    world->children = g_list_remove( world->children, this );
  
  if( callbacks ) g_hash_table_destroy( callbacks );

  g_hash_table_remove( StgModel::modelsbyid, (void*)id );

  world->RemoveModel( this );
}

// this should be called after all models have loaded from the
// worldfile - it's a chance to do any setup now that all models are
// in existence
void StgModel::Init()
{
  if( initfunc )
    Subscribe();
}  

void StgModel::AddFlag( StgFlag* flag )
{
  if( flag )
    flag_list = g_list_append( this->flag_list, flag );
}

void StgModel::RemoveFlag( StgFlag* flag )
{
  if( flag )
    flag_list = g_list_remove( this->flag_list, flag );
}

void StgModel::PushFlag( StgFlag* flag )
{
  if( flag )
    flag_list = g_list_prepend( flag_list, flag);
}

StgFlag* StgModel::PopFlag()
{
  if( flag_list == NULL )
    return NULL;

  StgFlag* flag = (StgFlag*)flag_list->data;
  flag_list = g_list_remove( flag_list, flag );

  return flag;
}


void StgModel::AddBlock( stg_point_t* pts, 
								 size_t pt_count,
								 stg_meters_t zmin,
								 stg_meters_t zmax,
								 stg_color_t col,
								 bool inherit_color )
{
  blocks = 
    g_list_prepend( blocks, new StgBlock( this, pts, pt_count, 
														zmin, zmax, 
														col, inherit_color ));

  // force recreation of display lists before drawing
  NeedRedraw();
}


void StgModel::ClearBlocks( void )
{
  stg_block_list_destroy( blocks );
  blocks = NULL;
  NeedRedraw();
}

void StgModel::AddBlockRect( double x, double y, 
									  double width, double height )
{  
  stg_point_t pts[4];
  pts[0].x = x;
  pts[0].y = y;
  pts[1].x = x + width;
  pts[1].y = y;
  pts[2].x = x + width;
  pts[2].y = y + height;
  pts[3].x = x;
  pts[3].y = y + height;

  // todo - fix this
  AddBlock( pts, 4, 0, 1, 0, true );	      
}


void StgModel::Raytrace( stg_pose_t pose,
								 stg_meters_t range, 
								 stg_block_match_func_t func,
								 const void* arg,
								 stg_raytrace_sample_t* sample,
								 bool ztest )
{
  world->Raytrace( LocalToGlobal(pose),
						 range,
						 func,
						 this,
						 arg,
						 sample,
						 ztest );
}

void StgModel::Raytrace( stg_radians_t bearing,
								 stg_meters_t range, 
								 stg_block_match_func_t func,
								 const void* arg,
								 stg_raytrace_sample_t* sample,
								 bool ztest )
{
  stg_pose_t raystart;
  bzero( &raystart, sizeof(raystart));
  raystart.a = bearing;

  world->Raytrace( LocalToGlobal(raystart),
						 range,
						 func,
						 this,
						 arg,
						 sample,
						 ztest );
}


void StgModel::Raytrace( stg_radians_t bearing,
								 stg_meters_t range, 
								 stg_radians_t fov,
								 stg_block_match_func_t func,
								 const void* arg,
								 stg_raytrace_sample_t* samples,
								 uint32_t sample_count,
								 bool ztest )
{
  stg_pose_t raystart;
  bzero( &raystart, sizeof(raystart));
  raystart.a = bearing;

  world->Raytrace( LocalToGlobal(raystart),
						 range,		   
						 fov,
						 func,
						 this,
						 arg,
						 samples,
						 sample_count,
						 ztest );
}

// utility for g_free()ing everything in a list
void list_gfree( GList* list )
{
  // free all the data in the list
  LISTFUNCTION( list, gpointer, g_free );

  // free the list elements themselves
  g_list_free( list );
}

// convert a global pose into the model's local coordinate system
void StgModel::GlobalToLocal( stg_pose_t* pose )
{
  //printf( "g2l global pose %.2f %.2f %.2f\n",
  //  pose->x, pose->y, pose->a );

  // get model's global pose
  stg_pose_t org = GetGlobalPose();

  //printf( "g2l global origin %.2f %.2f %.2f\n",
  //  org.x, org.y, org.a );

  // compute global pose in local coords
  double sx =  (pose->x - org.x) * cos(org.a) + (pose->y - org.y) * sin(org.a);
  double sy = -(pose->x - org.x) * sin(org.a) + (pose->y - org.y) * cos(org.a);
  double sa = pose->a - org.a;

  pose->x = sx;
  pose->y = sy;
  pose->a = sa;

  //printf( "g2l local pose %.2f %.2f %.2f\n",
  //  pose->x, pose->y, pose->a );
}

void StgModel::Say( const char* str )
{
  if( say_string )
    free( say_string );
  say_string = strdup( str );
}

bool StgModel::IsAntecedent( StgModel* testmod )
{
  if( this == testmod )
    return true;

  if( this->Parent()->IsAntecedent( testmod ) )
    return true;

  // neither mod nor a child of mod matches testmod
  return false;
}

// returns true if model [testmod] is a descendent of model [mod]
bool StgModel::IsDescendent( StgModel* testmod )
{
  if( this == testmod )
    return true;

  for( GList* it=this->children; it; it=it->next )
    {
      StgModel* child = (StgModel*)it->data;
      if( child->IsDescendent( testmod ) )
		  return true;
    }

  // neither mod nor a child of mod matches testmod
  return false;
}

// returns 1 if model [mod1] and [mod2] are in the same model tree
bool StgModel::IsRelated( StgModel* mod2 )
{
  if( this == mod2 )
    return true;

  // find the top-level model above mod1;
  StgModel* t = this;
  while( t->Parent() )
    t = t->Parent();

  // now seek mod2 below t
  return t->IsDescendent( mod2 );
}

// get the model's velocity in the global frame
stg_velocity_t StgModel::GetGlobalVelocity()
{
  stg_pose_t gpose = GetGlobalPose();

  double cosa = cos( gpose.a );
  double sina = sin( gpose.a );

  stg_velocity_t gv;
  gv.x = velocity.x * cosa - velocity.y * sina;
  gv.y = velocity.x * sina + velocity.y * cosa;
  gv.a = velocity.a;

  //printf( "local velocity %.2f %.2f %.2f\nglobal velocity %.2f %.2f %.2f\n",
  //  mod->velocity.x, mod->velocity.y, mod->velocity.a,
  //  gvel->x, gvel->y, gvel->a );

  return gv;
}

// set the model's velocity in the global frame
void StgModel::SetGlobalVelocity( stg_velocity_t gv )
{
  stg_pose_t gpose = GetGlobalPose();

  double cosa = cos( gpose.a );
  double sina = sin( gpose.a );

  stg_velocity_t lv;
  lv.x = gv.x * cosa + gv.y * sina;
  lv.y = -gv.x * sina + gv.y * cosa;
  lv.a = gv.a;

  this->SetVelocity( lv );
}

// get the model's position in the global frame
stg_pose_t StgModel::GetGlobalPose()
{ 
  //printf( "model %s global pose ", token );

  // if( this->gpose_dirty )
  {
	 stg_pose_t parent_pose;

	 // find my parent's pose
	 if( this->parent )
		{
		  parent_pose = parent->GetGlobalPose();	  
		  stg_pose_sum( &global_pose, &parent_pose, &pose );
			 
		  // we are on top of our parent
		  global_pose.z += parent->geom.size.z;
		}
	 else
		memcpy( &global_pose, &pose, sizeof(stg_pose_t));
		
	 this->gpose_dirty = false;
	 //printf( " WORK " );
  }
  //else
  //printf( " CACHED " );

  //   PRINT_DEBUG4( "GET GLOBAL POSE [x:%.2f y:%.2f z:%.2f a:%.2f]",
  // 		global_pose.x,
  // 		global_pose.y,
  // 		global_pose.z,
  // 		global_pose.a );

  return global_pose;
}


// convert a pose in this model's local coordinates into global
// coordinates
// should one day do all this with affine transforms for neatness?
stg_pose_t StgModel::LocalToGlobal( stg_pose_t pose )
{  
  return pose_sum( pose_sum( GetGlobalPose(), geom.pose ), pose );
}

stg_point3_t StgModel::LocalToGlobal( stg_point3_t point )
{
  stg_pose_t pose;
  pose.x = point.x;
  pose.y = point.y;
  pose.z = point.z;
  pose.a = 0;

  pose = LocalToGlobal( pose );

  point.x = pose.x;
  point.y = pose.y;
  point.z = pose.z;

  return point;
}

void StgModel::MapWithChildren()
{
  Map();

  // recursive call for all the model's children
  for( GList* it=children; it; it=it->next )
    ((StgModel*)it->data)->MapWithChildren();

}

void StgModel::UnMapWithChildren()
{
  UnMap();

  // recursive call for all the model's children
  for( GList* it=children; it; it=it->next )
    ((StgModel*)it->data)->UnMapWithChildren();
}

void StgModel::Map()
{
  //PRINT_DEBUG1( "%s.Map()", token );

  //   if( world->graphics && this->debug )
  //     {
  //       double scale = 1.0 / world->ppm;
  //       glPushMatrix();
  //       glTranslatef( 0,0,1 );
  //       glScalef( scale,scale,scale );
  //     }

  for( GList* it=blocks; it; it=it->next )
    ((StgBlock*)it->data)->Map();

  //   if( world->graphics && this->debug )
  //     glPopMatrix();
} 

void StgModel::UnMap()
{
  //PRINT_DEBUG1( "%s.UnMap()", token );

  for( GList* it=blocks; it; it=it->next )
    ((StgBlock*)it->data)->UnMap();
}


void StgModel::Subscribe( void )
{
  subs++;
  world->total_subs++;

  //printf( "subscribe to %s %d\n", token, subs );

  // if this is the first sub, call startup
  if( this->subs == 1 )
    this->Startup();
}

void StgModel::Unsubscribe( void )
{
  subs--;
  world->total_subs--;

  printf( "unsubscribe from %s %d\n", token, subs );

  // if this is the last sub, call shutdown
  if( subs < 1 )
    this->Shutdown();
}


void pose_invert( stg_pose_t* pose )
{
  pose->x = -pose->x;
  pose->y = -pose->y;
  pose->a = pose->a;
}

void StgModel::Print( char* prefix )
{
  if( prefix )
    printf( "%s model ", prefix );
  else
    printf( "Model ");

  printf( "%s:%s\n", 
			 //			id, 
			 world->Token(), 
			 token );

  for( GList* it=children; it; it=it->next )
    ((StgModel*)it->data)->Print( prefix );
}

const char* StgModel::PrintWithPose()
{
  stg_pose_t gpose = GetGlobalPose();

  static char txt[256];
  snprintf(txt, sizeof(txt), "%s @ [%.2f,%.2f,%.2f,%.2f]",  
			  token, 
			  gpose.x, gpose.y, gpose.z, gpose.a  );

  return txt;
}



void StgModel::Startup( void )
{
  //printf( "Startup model %s\n", this->token );

  // TODO:  this could be a callback
  if( initfunc )
    initfunc( this );

  world->StartUpdatingModel( this );

  CallCallbacks( &startup_hook );
}

void StgModel::Shutdown( void )
{
  //printf( "Shutdown model %s\n", this->token );

  world->StopUpdatingModel( this );

  CallCallbacks( &shutdown_hook );
}

void StgModel::UpdateIfDue( void )
{
  if(  world->sim_time  >= 
       (last_update + interval) )
    this->Update();
}

void StgModel::Update( void )
{
  //printf( "[%llu] %s update (%d subs)\n", 
  //  this->world->sim_time, this->token, this->subs );
  
  CallCallbacks( &update_hook );
  last_update = world->sim_time;
}

void StgModel::DrawSelected()
{
  glPushMatrix();

  glTranslatef( pose.x, pose.y, pose.z+0.01 ); // tiny Z offset raises rect above grid

  stg_pose_t gpose = GetGlobalPose();

  char buf[64];
  snprintf( buf, 63, "%s [%.2f %.2f %.2f %.2f]", 
				token, gpose.x, gpose.y, gpose.z, rtod(gpose.a) );

  PushColor( 0,0,0,1 ); // text color black
  gl_draw_string( 0.5,0.5,0.5, buf );

  glRotatef( rtod(pose.a), 0,0,1 );

  gl_pose_shift( &geom.pose );

  double dx = geom.size.x / 2.0 * 1.6;
  double dy = geom.size.y / 2.0 * 1.6;

  PopColor();
  PushColor( 1,0,0,0.8 ); // highlight color red
  glRectf( -dx, -dy, dx, dy );

  PopColor();
  glPopMatrix();
}


void StgModel::DrawTrailFootprint()
{
  double r,g,b,a;

  for( int i=trail->len-1; i>=0; i-- )
    {
      stg_trail_item_t* checkpoint = & g_array_index( trail, stg_trail_item_t, i );

      glPushMatrix();
      gl_pose_shift( &checkpoint->pose );
      gl_pose_shift( &geom.pose );

      stg_color_unpack( checkpoint->color, &r, &g, &b, &a );
      PushColor( r, g, b, 0.1 );

      LISTMETHOD( this->blocks, StgBlock*, DrawFootPrint );

      glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
      PushColor( r/2, g/2, b/2, 0.1 );
      LISTMETHOD( this->blocks, StgBlock*, DrawFootPrint );

      PopColor();
      PopColor();
	  glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
      glPopMatrix();
    }
}

void StgModel::DrawTrailBlocks()
{
  double timescale = 0.0000001;

  for( int i=trail->len-1; i>=0; i-- )
    {
      stg_trail_item_t* checkpoint = & g_array_index( trail, stg_trail_item_t, i );

      stg_pose_t pose;
      memcpy( &pose, &checkpoint->pose, sizeof(pose));
      pose.z =  (world->sim_time - checkpoint->time) * timescale;

      PushLocalCoords();
      DrawBlocks();
      PopCoords();
    }
}

void StgModel::DrawTrailArrows()
{
  double r,g,b,a;

  double dx = 0.2;
  double dy = 0.07;
  double timescale = 0.0000001;

  for( unsigned int i=0; i<trail->len; i++ )
    {
      stg_trail_item_t* checkpoint = & g_array_index( trail, stg_trail_item_t, i );

      stg_pose_t pose;
      memcpy( &pose, &checkpoint->pose, sizeof(pose));
      pose.z =  (world->sim_time - checkpoint->time) * timescale;

      PushLocalCoords();

      // set the height proportional to age

      PushColor( checkpoint->color );

      glEnable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(1.0, 1.0);

      glBegin( GL_TRIANGLES );
       glVertex3f( 0, -dy, 0);
       glVertex3f( dx, 0, 0 );
       glVertex3f( 0, +dy, 0 );
      glEnd();
      glDisable(GL_POLYGON_OFFSET_FILL);

      glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

      stg_color_unpack( checkpoint->color, &r, &g, &b, &a );
      PushColor( r/2, g/2, b/2, 1 ); // darker color

      glDepthMask(GL_FALSE);
      glBegin( GL_TRIANGLES );
       glVertex3f( 0, -dy, 0);
       glVertex3f( dx, 0, 0 );
       glVertex3f( 0, +dy, 0 );
      glEnd();
      glDepthMask(GL_TRUE);

	  glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
      PopColor();
      PopColor();
      PopCoords();
    }
}

void StgModel::PushMyPose()
{
  world->PushPose();
  world->ShiftPose( &pose );
}

void StgModel::PopPose()
{
  world->PopPose();
}

void StgModel::ShiftPose( stg_pose_t* pose )
{
  world->ShiftPose( pose );
}

void StgModel::ShiftToTop()
{
  stg_pose_t top;
  bzero( &top, sizeof(top));  
  top.z = geom.size.z;
  ShiftPose( &top );
}

void StgModel::DrawOriginTree()
{
  PushMyPose();

  world->DrawPose();
  
  ShiftToTop();

  for( GList* it=children; it; it=it->next )
	 ((StgModel*)it->data)->DrawOriginTree();
  
  PopPose();
}
 
void StgWorld::DrawPose()
{
  PushColor( 0,0,0,1 );
  glPointSize( 4 );
  
  stg_pose_t* gpose = PeekPose();
  
  glBegin( GL_POINTS );
  glVertex3f( gpose->x, gpose->y, gpose->z );
  glEnd();
  
  PopColor();  
}


void StgModel::DrawBlocksTree( )
{
  PushLocalCoords();

  LISTMETHOD( children, StgModel*, DrawBlocksTree );

  DrawBlocks();
  
  PopCoords();
}
  

void StgModel::DrawBlocks( )
{
  // testing - draw bounding box
//   PushColor( color );

//   // bottom
//   glBegin( GL_LINE_LOOP );
//   glVertex3f( -geom.size.x/2.0, -geom.size.y/2.0, 0 );
//   glVertex3f( +geom.size.x/2.0, -geom.size.y/2.0, 0 );
//   glVertex3f( +geom.size.x/2.0, +geom.size.y/2.0, 0 );
//   glVertex3f( -geom.size.x/2.0, +geom.size.y/2.0, 0 );
//   glEnd();

//   // top
//   glBegin( GL_LINE_LOOP );
//   glVertex3f( -geom.size.x/2.0, -geom.size.y/2.0, geom.size.z );
//   glVertex3f( +geom.size.x/2.0, -geom.size.y/2.0, geom.size.z );
//   glVertex3f( +geom.size.x/2.0, +geom.size.y/2.0, geom.size.z );
//   glVertex3f( -geom.size.x/2.0, +geom.size.y/2.0, geom.size.z );
//   glEnd();

//   PopColor();

  // TODO - fix this!
  //if( rebuild_displaylist )
	 {
		rebuild_displaylist = false;
      
		glNewList( blocks_dl, GL_COMPILE );	

		gl_pose_shift( &geom.pose );

		LISTMETHOD( this->blocks, StgBlock*, Draw );
		glEndList();
	 }
  
	 //printf( "calling list for %s\n", token );
  glCallList( blocks_dl );
}

// move into this model's local coordinate frame
void StgModel::PushLocalCoords()
{
  glPushMatrix();  
  
  if( parent )
	 glTranslatef( 0,0, parent->geom.size.z );
  
  gl_pose_shift( &pose );


  // useful debug - draw a point at the local origin
 //  PushColor( color );
//   glPointSize( 5.0 );
//   glBegin( GL_POINTS );
//   glVertex2i( 0, 0 );
//   glEnd();
//   PopColor();
}

void StgModel::PopCoords()
{
  glPopMatrix();
}

void StgModel::DrawStatusTree( StgCanvas* canvas ) 
{
  PushLocalCoords();
  DrawStatus( canvas );
  LISTMETHODARG( children, StgModel*, DrawStatusTree, canvas );  
  PopCoords();
}

void StgModel::DrawStatus( StgCanvas* canvas ) 
{
	if( say_string )	  
	{
		float yaw, pitch;
		pitch = - canvas->current_camera->pitch();
		yaw = - canvas->current_camera->yaw();			
		
		float robotAngle = -rtod(pose.a);
		glPushMatrix();
		
		//TODO ask jeremy to fancy up the octagons
		//const float m = 4; // margin
		
		float w = gl_width( this->say_string ); // scaled text width
		float h = gl_height(); // scaled text height
		
		// move above the robot
		glTranslatef( 0, 0, 0.5 );		
		
		// rotate to face screen
		glRotatef( robotAngle - yaw, 0,0,1 );
		glRotatef( -pitch, 1,0,0 );
		
		//get raster positition, add gl_width, then project back to world coords
		glRasterPos3f( 0, 0, 0 );
		GLfloat pos[ 4 ];
		glGetFloatv(GL_CURRENT_RASTER_POSITION, pos);
		
		GLboolean valid;
		glGetBooleanv( GL_CURRENT_RASTER_POSITION_VALID, &valid );
		if( valid == true ) {
			
			{
				GLdouble wx, wy, wz;
				int viewport[4];
				glGetIntegerv(GL_VIEWPORT, viewport);
				
				GLdouble modelview[16];
				glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
				
				GLdouble projection[16];	
				glGetDoublev(GL_PROJECTION_MATRIX, projection);
				
				//get width and height in world coords
				gluUnProject( pos[0] + w, pos[1], pos[2], modelview, projection, viewport, &wx, &wy, &wz );
				w = wx;
				gluUnProject( pos[0], pos[1] + h, pos[2], modelview, projection, viewport, &wx, &wy, &wz );
				h = wy;
			}
			
			glColor3f( 1, 0.8, 1 );
			gl_draw_octagon( w, h, 0 );
			
			glColor3f( 0, 0, 0 );
			//position text ontop of string (might be problematic if too large in perspective mode)
			glTranslatef( 0, 0, 0.003 );
			gl_draw_string( 0, 0, 0.0, this->say_string );
			
			glPopMatrix();
		}
    }
	
	if( stall )
    {
		DrawImage( TextureManager::getInstance()._stall_texture_id, canvas, 0.85 );
    }
}

void StgModel::DrawImage( uint32_t texture_id, Stg::StgCanvas* canvas, float alpha )
{
  float stheta = dtor( canvas->current_camera->pitch() );
  float sphi = - dtor( canvas->current_camera->yaw() );
  if( canvas->pCamOn == true ) {
	 sphi = atan2(
					  ( pose.x - canvas->current_camera->x() )
					  ,
					  ( pose.y - canvas->current_camera->y() )
					  );
  }

  glEnable(GL_TEXTURE_2D);
  glBindTexture( GL_TEXTURE_2D, texture_id );

  glColor4f( 1.0, 1.0, 1.0, alpha );
  glPushMatrix();

  glTranslatef( 0.0, 0.0, 0.75 );

  //orient 2d sprites to face the camera (left-right)
  float a = rtod( sphi + pose.a );
  glRotatef( -a, 0.0, 0.0, 1.0 );

  //orient to face camera (from top-front)
  a = rtod( stheta );
  glRotatef( -(90.0-a), 1.0, 0.0, 0.0 );

  //draw a square, with the textured image
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.25f, 0, -0.25f );
  glTexCoord2f(1.0f, 0.0f); glVertex3f( 0.25f, 0, -0.25f );
  glTexCoord2f(1.0f, 1.0f); glVertex3f( 0.25f, 0,  0.25f );
  glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.25f, 0,  0.25f );
  glEnd();

  glPopMatrix();
  glBindTexture( GL_TEXTURE_2D, 0 );
  glDisable(GL_TEXTURE_2D);
}

void StgModel::DrawFlagList( void )
{	
  if( flag_list  == NULL )
	 return;
  
  PushLocalCoords();
  
  GLUquadric* quadric = gluNewQuadric();
  glTranslatef(0,0,1); // jump up
  stg_pose_t gpose = GetGlobalPose();
  glRotatef( 180 + rtod(-gpose.a),0,0,1 );
  
  
  GList* list = g_list_copy( flag_list );
  list = g_list_reverse(list);
  
  for( GList* item = list; item; item = item->next )
	 {
		
		StgFlag* flag = (StgFlag*)item->data;
		
		glTranslatef( 0, 0, flag->size/2.0 );
		
		
		PushColor( flag->color );
		
		gluQuadricDrawStyle( quadric, GLU_FILL );
		gluSphere( quadric, flag->size/2.0, 4,2  );
		
		// draw the edges darker version of the same color
		double r,g,b,a;
		stg_color_unpack( flag->color, &r, &g, &b, &a );
		PushColor( stg_color_pack( r/2.0, g/2.0, b/2.0, a ));
		
		gluQuadricDrawStyle( quadric, GLU_LINE );
		gluSphere( quadric, flag->size/2.0, 4,2 );
		
		PopColor();
		PopColor();
		
		glTranslatef( 0, 0, flag->size/2.0 );
	 }
  
  g_list_free( list );
  
  gluDeleteQuadric( quadric );
  
  PopCoords();
}


void StgModel::DrawBlinkenlights()
{
  PushLocalCoords();

  GLUquadric* quadric = gluNewQuadric();
  //glTranslatef(0,0,1); // jump up
  //stg_pose_t gpose = GetGlobalPose();
  //glRotatef( 180 + rtod(-gpose.a),0,0,1 );

  for( unsigned int i=0; i<blinkenlights->len; i++ )
    {
      stg_blinkenlight_t* b = 
		  (stg_blinkenlight_t*)g_ptr_array_index( blinkenlights, i );
      assert(b);

      glTranslatef( b->pose.x, b->pose.y, b->pose.z );

      PushColor( b->color );

      if( b->enabled )
		  gluQuadricDrawStyle( quadric, GLU_FILL );
      else
		  gluQuadricDrawStyle( quadric, GLU_LINE );

      gluSphere( quadric, b->size/2.0, 8,8  );

      PopColor();
    }

  gluDeleteQuadric( quadric );

  PopCoords();
}

void StgModel::DrawPicker( void )
{
  //PRINT_DEBUG1( "Drawing %s", token );
  PushLocalCoords();

  // draw the boxes
  for( GList* it=blocks; it; it=it->next )
    ((StgBlock*)it->data)->DrawSolid() ;

  // shift up the CS to the top of this model
  //gl_coord_shift(  0,0, this->geom.size.z, 0 );

  // recursively draw the tree below this model 
  LISTMETHOD( this->children, StgModel*, DrawPicker );

  PopCoords();
}

void StgModel::DataVisualize( void )
{  
  // do nothing
}

void StgModel::DataVisualizeTree( void )
{
  PushLocalCoords();
  DataVisualize(); // virtual function overridden by most model types  

  // and draw the children
  LISTMETHOD( children, StgModel*, DataVisualizeTree );

  PopCoords();
}


void StgModel::DrawGrid( void )
{
  if ( gui_grid ) 
    {
      PushLocalCoords();
		
      stg_bounds3d_t vol;
      vol.x.min = -geom.size.x/2.0;
      vol.x.max =  geom.size.x/2.0;
      vol.y.min = -geom.size.y/2.0;
      vol.y.max =  geom.size.y/2.0;
      vol.z.min = 0;
      vol.z.max = geom.size.z;
		 
      PushColor( 0,0,1,0.4 );
      gl_draw_grid(vol);
      PopColor();		 
      PopCoords();
    }
}

bool velocity_is_nonzero( stg_velocity_t* v )
{
  return( v->x || v->y || v->z || v->a );
}

void StgModel::SetVelocity( stg_velocity_t vel )
{
  this->velocity = vel;

  if( ! this->on_velocity_list && velocity_is_nonzero( & this->velocity ) )
    {
      this->world->velocity_list = g_list_prepend( this->world->velocity_list, this );
      this->on_velocity_list = true;
    }

  if( this->on_velocity_list && ! velocity_is_nonzero( &this->velocity ))
    {
      this->world->velocity_list = g_list_remove( this->world->velocity_list, this );
      this->on_velocity_list = false;
    }    

  CallCallbacks( &this->velocity );
}

void StgModel::NeedRedraw( void )
{
  this->rebuild_displaylist = true;

  if( parent )
    parent->NeedRedraw();
}

void StgModel::GPoseDirtyTree( void )
{
  this->gpose_dirty = true; // our global pose may have changed

  for( GList* it = this->children; it; it=it->next )
    ((StgModel*)it->data)->GPoseDirtyTree();
}

// find the Z-axis offset of this model, by recursively summing the
// height of its parents and any Z poses shifts
// stg_meters_t StgModel::BaseHeight()
// {
//   if( ! parent )
// 	 return pose.z;

//   return pose.z + parent->geom.size.z + parent->BaseHeight();
// }


void StgModel::SetPose( stg_pose_t pose )
{
  //PRINT_DEBUG5( "%s.SetPose(%.2f %.2f %.2f %.2f)", 
  //	this->token, pose->x, pose->y, pose->z, pose->a );

  // if the pose has changed, we need to do some work
  if( memcmp( &this->pose, &pose, sizeof(stg_pose_t) ) != 0 )
    {
      UnMapWithChildren();

      pose.a = normalize( pose.a );
      this->pose = pose;
      this->pose.a = normalize(this->pose.a);

      this->NeedRedraw();
      this->GPoseDirtyTree(); // global pose may have changed

      MapWithChildren();
    }

  // register a model change even if the pose didn't actually change
  CallCallbacks( &this->pose );
}

void StgModel::AddToPose( double dx, double dy, double dz, double da )
{
  if( dx || dy || dz || da )
    {
      stg_pose_t pose = GetPose();
      pose.x += dx;
      pose.y += dy;
      pose.z += dz;
      pose.a += da;

      SetPose( pose );
    }
}

void StgModel::AddToPose( stg_pose_t pose )
{
  this->AddToPose( pose.x, pose.y, pose.z, pose.a );
}

void StgModel::SetGeom( stg_geom_t geom )
{
  //printf( "MODEL \"%s\" SET GEOM (%.2f %.2f %.2f)[%.2f %.2f]\n",
  //  this->token,
  //  geom->pose.x, geom->pose.y, geom->pose.a,
  //  geom->size.x, geom->size.y );

  this->gpose_dirty = true;

  UnMap();

  this->geom = geom;

  StgBlock::ScaleList( blocks, &geom.size );

  NeedRedraw();

  Map();

  CallCallbacks( &this->geom );
}

void StgModel::SetColor( stg_color_t col )
{
  this->color = col;
  NeedRedraw();
  CallCallbacks( &this->color );
}

void StgModel::SetMass( stg_kg_t mass )
{
  this->mass = mass;
  CallCallbacks( &this->mass );
}

void StgModel::SetStall( stg_bool_t stall )
{
  this->stall = stall;
  CallCallbacks( &this->stall );
}

void StgModel::SetGripperReturn( int val )
{
  this->gripper_return = val;
  CallCallbacks( &this->gripper_return );
}

void StgModel::SetFiducialReturn(  int val )
{
  this->fiducial_return = val;
  CallCallbacks( &this->fiducial_return );
}

void StgModel::SetFiducialKey( int key )
{
  this->fiducial_key = key;
  CallCallbacks( &this->fiducial_key );
}

void StgModel::SetLaserReturn( stg_laser_return_t val )
{
  this->laser_return = val;
  CallCallbacks( &this->laser_return );
}

void StgModel::SetObstacleReturn( int val )
{
  this->obstacle_return = val;
  CallCallbacks( &this->obstacle_return );
}

void StgModel::SetBlobReturn( int val )
{
  this->blob_return = val;
  CallCallbacks( &this->blob_return );
}

void StgModel::SetRangerReturn( int val )
{
  this->ranger_return = val;
  CallCallbacks( &this->ranger_return );
}

void StgModel::SetBoundary( int val )
{
  this->boundary = val;
  CallCallbacks( &this->boundary );
}

void StgModel::SetGuiNose(  int val )
{
  this->gui_nose = val;
  CallCallbacks( &this->gui_nose );
}

void StgModel::SetGuiMask( int val )
{
  this->gui_mask = val;
  CallCallbacks( &this->gui_mask );
}

void StgModel::SetGuiGrid(  int val )
{
  this->gui_grid = val;
  CallCallbacks( &this->gui_grid );
}

void StgModel::SetGuiOutline( int val )
{
  this->gui_outline = val;
  CallCallbacks( &this->gui_outline );
}

void StgModel::SetWatts(  stg_watts_t watts )
{
  this->watts = watts;
  CallCallbacks( &this->watts );
}

void StgModel::SetMapResolution(  stg_meters_t res )
{
  this->map_resolution = res;
  CallCallbacks( &this->map_resolution );
}

// set the pose of model in global coordinates 
void StgModel::SetGlobalPose( stg_pose_t gpose )
{
  if( this->parent == NULL )
    {
      //printf( "setting pose directly\n");
      this->SetPose( gpose );
    }  
  else
    {
      stg_pose_t lpose;
      memcpy( &lpose, &gpose, sizeof(lpose) );
      this->parent->GlobalToLocal( &lpose );
      this->SetPose( lpose );
    }

  //printf( "setting global pose %.2f %.2f %.2f = local pose %.2f %.2f %.2f\n",
  //      gpose->x, gpose->y, gpose->a, lpose.x, lpose.y, lpose.a );
}

int StgModel::SetParent(  StgModel* newparent)
{
  // remove the model from its old parent (if it has one)
  if( this->parent )
    this->parent->children = g_list_remove( this->parent->children, this );

  if( newparent )
    newparent->children = g_list_append( newparent->children, this );

  // link from the model to its new parent
  this->parent = newparent;

  CallCallbacks( &this->parent );
  return 0; //ok
}


static bool collision_match( StgBlock* testblock, 
									  StgModel* finder )
{ 
  return( (testblock->Model() != finder) && testblock->Model()->ObstacleReturn()  );
}	


void StgModel::PlaceInFreeSpace( stg_meters_t xmin, stg_meters_t xmax, 
											stg_meters_t ymin, stg_meters_t ymax )
{
  while( TestCollision( NULL, NULL ) )
    SetPose( random_pose( xmin,xmax, ymin, ymax ));		
}


StgModel* StgModel::TestCollision( stg_meters_t* hitx, stg_meters_t* hity )
{
  return TestCollision( new_pose(0,0,0,0), hitx, hity );
}

StgModel* StgModel::TestCollision( stg_pose_t posedelta, 
											  stg_meters_t* hitx,
											  stg_meters_t* hity ) 
{ 
  /*  stg_model_t* child_hit = NULL; */

  /*   for( GList* it=mod->children; it; it=it->next ) */
  /*     { */
  /*       stg_model_t* child = (stg_model_t*)it->data; */
  /*       child_hit = stg_model_test_collision( child, hitx, hity ); */
  /*       if( child_hit ) */
  /* 	return child_hit; */
  /*     } */

  // raytrace along all our blocks. expensive, but most vehicles 
  // will just be a few blocks, grippers 3 blocks, etc. not too bad. 

  // no blocks, no hit!
  if( this->blocks == NULL )
    return NULL;

  StgModel* hitmod = NULL;

  // unrender myself - avoids a lot of self-hits
  this->UnMap();

  // add the local geom offset
  //stg_pose_t local;
  //stg_pose_sum( &local, pose, &this->geom.pose );

  // loop over all blocks 
  for( GList* it = this->blocks; it; it=it->next )
    {
      StgBlock* b = (StgBlock*)it->data;

      unsigned int pt_count;
      stg_point_t *pts = b->Points( &pt_count );

      // loop over all edges of the block
      for( unsigned int p=0; p < pt_count; p++ ) 
		  { 
			 // find the local poses of the ends of this block edge
			 stg_point_t* pt1 = &pts[p];
			 stg_point_t* pt2 = &pts[(p+1) % pt_count];
			 double dx = pt2->x - pt1->x;
			 double dy = pt2->y - pt1->y;

			 // find the range and bearing to raytrace along the block edge
			 double range = hypot( dx, dy );
			 double bearing = atan2( dy,dx );

			 stg_pose_t edgepose;
			 edgepose.x = pt1->x;
			 edgepose.y = pt1->y;
             edgepose.z = 0;
			 edgepose.a = bearing;

			 // shift the edge ray vector by the local change in pose
			 // 	  stg_pose_t raypose;	  
			 // 	  stg_pose_sum( &raypose, posedelta, &edgepose );

			 // raytrace in local coordinates
			 stg_raytrace_sample_t sample;
			 Raytrace( pose_sum( posedelta, edgepose), 
						  range,
						  (stg_block_match_func_t)collision_match, 
						  NULL, 
						  &sample,
						  true );

			 if( sample.block )
				hitmod = sample.block->Model();	  
		  } 
    } 

  // re-render myself
  this->Map();
  return hitmod;  // done  
}


void StgModel::UpdatePose( void )
{
  if( disabled )
    return;

  // TODO - control this properly, and maybe do it faster
  //  if( 0 )
  if( (world->updates % 10 == 0) )
    {
      stg_trail_item_t checkpoint;
      memcpy( &checkpoint.pose, &pose, sizeof(pose));
      checkpoint.color = color;
      checkpoint.time = world->sim_time;

      if( trail->len > 100 )
		  g_array_remove_index( trail, 0 );

      g_array_append_val( this->trail, checkpoint );
    }

  // convert usec to sec
  //TODO make this static-ish
  double interval = (double)world->interval_sim / 1e6;

  // find the change of pose due to our velocity vector
  stg_pose_t p;
  p.x = velocity.x * interval;
  p.y = velocity.y * interval;
  p.z = 0;
  p.a = velocity.a * interval;

  // test to see if this pose change would cause us to crash
  StgModel* hitthing = this->TestCollision( p, NULL, NULL );

  //double hitx=0, hity=0;
  //StgModel* hitthing = this->TestCollision( &p, &hitx, &hity );

  if( hitthing )
    {
      // XX 
      SetStall( true );
      //printf( "hit %s at %.2f %.2f\n",
      //      hitthing->Token(), hitx, hity );

      //dd// 	  //memcpy( &mod->pose, &old_pose, sizeof(mod->pose));
      //this->SetPose( &old_pose );
    }
  else
    {
      // XX 
      SetStall( false );

      stg_pose_t newpose;
      stg_pose_sum( &newpose, &this->pose, &p );
      this->SetPose( newpose );
    }

  //   int stall = 0;

  //  if( hitthing )
  //   {
  //       // grippable objects move when we hit them
  //       if(  hitthing->gripper_return ) 
  // 	{
  // 	  //PRINT_WARN( "HIT something grippable!" );

  // 	  stg_velocity_t hitvel;
  // 	  //hitvel.x = gvel.x;
  // 	  //hitvel.y = gvel.y;
  // 	  //hitvel.a = 0.0;

  // 	  stg_pose_t hitpose;
  // 	  stg_model_get_global_pose( hitthing, &hitpose );
  // 	  double hita = atan2( hitpose.y - hity, hitpose.x - hitx );
  // 	  double mag = 0.3;
  // 	  hitvel.x =  mag * cos(hita);
  // 	  hitvel.y = mag * sin(hita);
  // 	  hitvel.a = 0;

  // 	  stg_model_set_global_velocity( hitthing, &hitvel );

  // 	  stall = 0;
  // 	  // don't make the move - reset the pose
  // 	  //memcpy( &mod->pose, &old_pose, sizeof(mod->pose));
  // 	}
  //       else // other objects block us totally
  // 	{
  // 	  hitthing = NULL;
  // 	  // move back to where we started
  // 	  memcpy( &mod->pose, &old_pose, sizeof(mod->pose));

  //     // compute a move over a smaller period of time
  //        interval = 0.1 * interval; // slow down time so we move slowly
  //        stg_pose_t p;
  //        bzero(&p,sizeof(p));
  //        p.x += (v.x * cos(p.a) - v.y * sin(p.a)) * interval;
  //        p.y += (v.x * sin(p.a) + v.y * cos(p.a)) * interval;
  //        p.a += v.a * interval;

  //        // inch forward until we collide
  //        while( (TestCollision( &p, NULL, NULL ) == NULL) );
  //        {
  // 	 puts( "inching" );
  // 	 stg_pose_t newpose;
  // 	 stg_pose_sum( &newpose, &this->pose, &p );
  // 	 this->SetPose( &newpose );
  //        }


  // // 	  //PRINT_WARN( "HIT something immovable!" );

  //  	  stall = 1;

  // 	  // set velocity to zero
  // 	  stg_velocity_t zero_v;
  // 	  memset( &zero_v, 0, sizeof(zero_v));
  // 	  stg_model_set_velocity( mod, &zero_v );

  // 	  // don't make the move - reset the pose
  // 	  memcpy( &mod->pose, &old_pose, sizeof(mod->pose));

  // 	}
  //  }

  //   // if the pose changed, we need to set it.
  //   if( memcmp( &old_pose, &mod->pose, sizeof(old_pose) ))
  //       stg_model_set_pose( mod, &mod->pose );

  //   stg_model_set_stall( mod, stall );

  //   /* trails */
  //   //if( fig_trails )
  //   //gui_model_trail()


  //   // if I'm a pucky thing, slow down for next time - simulates friction
  //   if( mod->gripper_return )
  //     {
  //       double slow = 0.08;

  //       if( mod->velocity.x > 0 )
  // 	{
  // 	  mod->velocity.x -= slow;
  // 	  mod->velocity.x = MAX( 0, mod->velocity.x );
  // 	}
  //       else if ( mod->velocity.x < 0 )
  // 	{
  // 	  mod->velocity.x += slow;
  // 	  mod->velocity.x = MIN( 0, mod->velocity.x );
  // 	}

  //       if( mod->velocity.y > 0 )
  // 	{
  // 	  mod->velocity.y -= slow;
  // 	  mod->velocity.y = MAX( 0, mod->velocity.y );
  // 	}
  //       else if( mod->velocity.y < 0 )
  // 	{
  // 	  mod->velocity.y += slow;
  // 	  mod->velocity.y = MIN( 0, mod->velocity.y );
  // 	}

  //       CallCallbacks( &this->velocity );
  //     }

  //   return 0; // ok
}


int StgModel::TreeToPtrArray( GPtrArray* array )
{
  g_ptr_array_add( array, this );

  //printf( " added %s to array at %p\n", root->token, array );

  int added = 1;

  for(GList* it=children; it; it=it->next )
    added += ((StgModel*)it->data)->TreeToPtrArray( array );

  return added;
}

StgModel* StgModel::GetUnsubscribedModelOfType( stg_model_type_t type )
{
  printf( "searching for type %d in model %s type %d\n", type, token, this->type );
  
  if( (this->type == type) && (this->subs == 0) )
    return this;

  // this model is no use. try children recursively
  for( GList* it = children; it; it=it->next )
    {
      StgModel* child = (StgModel*)it->data;

      StgModel* found = child->GetUnsubscribedModelOfType( type );
      if( found )
		  return found;
    }

  // nothing matching below this model
  return NULL;
}

StgModel* StgModel::GetModel( const char* modelname )
{
  // construct the full model name and look it up
  char* buf = new char[TOKEN_MAX];
  snprintf( buf, TOKEN_MAX, "%s.%s", this->token, modelname );

  StgModel* mod = world->GetModel( buf );

  if( mod == NULL )
    PRINT_WARN1( "Model %s not found", buf );

  delete[] buf;

  return mod;
}
