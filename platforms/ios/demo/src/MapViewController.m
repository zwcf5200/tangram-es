//
//  MapViewController.m
//
//  Created by Karim Naaji on 10/12/16.
//  Copyright © 2016 Karim Naaji. All rights reserved.
//

#import "MapViewController.h"

@interface MapViewController ()

@property (assign, nonatomic) TGMarker* markerPolygon;
@property (strong, nonatomic) TGMapData* mapData;

- (void)addAlert:(NSString *)message withTitle:(NSString *)title;

@end

@implementation MapViewControllerDelegate

- (void)mapViewDidCompleteLoading:(TGMapViewController *)mapView
{
    NSLog(@"Did complete view");
}

- (void)mapView:(TGMapViewController *)mapView didLoadSceneAsync:(NSString *)scene
{
    NSLog(@"Did load scene async %@", scene);

    TGGeoPoint newYork;
    newYork.longitude = -74.00976419448854;
    newYork.latitude = 40.70532700869127;

    TGGeoPoint cairo;
    cairo.longitude = 30.00;
    cairo.latitude = 31.25;

    [mapView setZoom:15];
    [mapView setPosition:newYork];

    // Add a client data source, named 'mz_route_line_transit'
    MapViewController* vc = (MapViewController *)mapView;
    vc.mapData = [mapView addDataLayer:@"mz_route_line_transit"];
}

- (void)mapView:(TGMapViewController *)mapView didSelectMarker:(TGMarkerPickResult *)markerPickResult atScreenPosition:(TGGeoPoint)position;
{
    if (!markerPickResult) {
        return;
    }

    NSString* message = [NSString stringWithFormat:@"Marker %f %f",
        markerPickResult.marker.point.latitude,
        markerPickResult.marker.point.longitude];

    [(MapViewController*)mapView addAlert:message withTitle:@"Marker pick callback"];
}

- (void)mapView:(TGMapViewController *)mapView didSelectLabel:(TGLabelPickResult *)labelPickResult atScreenPosition:(CGPoint)position
{
    if (!labelPickResult) { return; }

    NSLog(@"Picked label:");

    for (NSString* key in [labelPickResult properties]) {
        NSLog(@"\t%@ -- %@", key, [[labelPickResult properties] objectForKey:key]);

        if ([key isEqualToString:@"name"]) {
            [(MapViewController*)mapView addAlert:[[labelPickResult properties] objectForKey:key] withTitle:@"Label selection callback"];
        }
    }
}

- (void)mapView:(TGMapViewController *)mapView didSelectFeature:(NSDictionary *)feature atScreenPosition:(CGPoint)position
{
    // Not feature selected
    if (!feature) { return; }

    NSLog(@"Picked features:");

    for (id key in feature) {
        NSLog(@"\t%@ -- %@", key, [feature objectForKey:key]);

        if ([key isEqualToString:@"name"]) {
            [(MapViewController*)mapView addAlert:[[feature objectForKey:key] objectForKey:key] withTitle:@"Feature selection callback"];
        }
    }
}

@end

@implementation MapViewControllerRecognizerDelegate

@synthesize added;

- (void)mapView:(TGMapViewController *)view recognizer:(UIGestureRecognizer *)recognizer didRecognizeSingleTapGesture:(CGPoint)location
{
  NSLog(@"Did tap at %f %f", location.x, location.y);
  [self addMarkerAndHandlePress:view location:location];
}

- (void)mapView:(TGMapViewController *)view recognizer:(UIGestureRecognizer *)recognizer didRecognizeLongPressGesture:(CGPoint)location
{
    NSLog(@"Did long press at %f %f", location.x, location.y);
  [self addMarkerAndHandlePress:view location:location];
}

- (void)addMarkerAndHandlePress:(TGMapViewController *) view location:(CGPoint) location {

  MapViewController* vc = (MapViewController *)view;

  if (!self.added) {
    self.added = true;

    TGGeoPoint coordinates = [vc screenPositionToLngLat:location];

    // Add polyline data layer
    {
      TGFeatureProperties* properties = @{ @"type" : @"line", @"color" : @"#D2655F" };
      static TGGeoPoint lastCoordinates = {NAN, NAN};

      if (!isnan(lastCoordinates.latitude)) {
        TGGeoPolyline* line = [[TGGeoPolyline alloc] init];

        [line addPoint:lastCoordinates];
        [line addPoint:coordinates];

        [vc.mapData addPolyline:line withProperties:properties];
      }

      lastCoordinates = coordinates;
    }

    // Add polygon marker
    {
      if (!vc.markerPolygon) {
        vc.markerPolygon = [[TGMarker alloc] init];
        vc.markerPolygon.styling = @"{ style: 'polygons', color: 'blue', order: 500 }";

        // Add the marker to the current view
        vc.markerPolygon.map = view;
      }

      static TGGeoPolygon* polygon = nil;
      if (!polygon) { polygon = [[TGGeoPolygon alloc] init]; }

      if ([polygon count] == 0) {
        [polygon startPath:coordinates withSize:5];
      } else if ([polygon count] % 5 == 0) {
        vc.markerPolygon.polygon = polygon;
        [polygon removeAll];
        [polygon startPath:coordinates withSize:5];
      } else {
        [polygon addPoint:coordinates];
      }
    }

    // Add point marker
    {
      TGMarker* markerPoint = [[TGMarker alloc] initWithMapView:view];
      markerPoint.styling = @"{ style: 'points', color: 'white', size: [50px, 50px], collide: false, interactive: true }";
      markerPoint.point = coordinates;
    }
    // Request feature picking
    [vc pickFeatureAt:location];
    [vc pickLabelAt:location];
  } else {
    [vc pickMarkerAt:location];
  }
}

@end

@implementation MapViewController

- (void)addAlert:(NSString *)message withTitle:(NSString *)title
{
    UIAlertController *alert = [[UIAlertController alloc] init];

    alert.title = title;
    alert.message = message;

    UIAlertAction* okAction = [UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleCancel handler:nil];
    [alert addAction:okAction];

    [self presentViewController:alert animated:YES completion:nil];
}

- (void)viewWillAppear:(BOOL)animated
{
    [super loadSceneFileAsync:@"https://mapzen.com/carto/bubble-wrap-style/bubble-wrap.yaml"];
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    NSLog(@"Sarah View height before %f", self.view.frame.size.height);
    self.mapViewDelegate = [[MapViewControllerDelegate alloc] init];
    self.gestureDelegate = [[MapViewControllerRecognizerDelegate alloc] init];
}

-(void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  NSLog(@"Sarah View height %f", self.view.frame.size.height);
}

-(void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator {
  NSLog(@"Sarah Will transition to size: %f", size.height);
}

-(void)glkView:(GLKView *)view drawInRect:(CGRect)rect {
  [super glkView:view drawInRect:rect];
  NSLog(@"Sarah draw in rect: %f", rect.size.height);
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
}

@end
