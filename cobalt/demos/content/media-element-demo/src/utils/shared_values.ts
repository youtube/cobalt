/** This file contains singleton values shared by the application. */
import {Observable} from './observable';

export const errors = new Observable<string[]>([]);
export const isCobalt = navigator.userAgent.indexOf('Cobalt') >= 0;
