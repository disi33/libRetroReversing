import React, { Fragment, useState, useEffect } from "react";
import ReactDOM from "react-dom";
import clsx from 'clsx';
import CssBaseline from '@material-ui/core/CssBaseline';

import Button from "@material-ui/core/Button";
import AppBar from '@material-ui/core/AppBar';
import Toolbar from '@material-ui/core/Toolbar';
import Typography from '@material-ui/core/Typography';
import Divider from '@material-ui/core/Divider';
import Drawer from '@material-ui/core/Drawer';
import List from '@material-ui/core/List';
import ListItem from '@material-ui/core/ListItem';
import ListItemIcon from '@material-ui/core/ListItemIcon';
import ListItemText from '@material-ui/core/ListItemText';
import Hidden from '@material-ui/core/Hidden';
import IconButton from '@material-ui/core/IconButton';
// Icons
import MailIcon from '@material-ui/icons/Mail';
import MenuIcon from '@material-ui/icons/Menu';
import InboxIcon from '@material-ui/icons/MoveToInbox';
import ChevronLeftIcon from '@material-ui/icons/ChevronLeft';
import ChevronRightIcon from '@material-ui/icons/ChevronRight';
import MemoryIcon from '@material-ui/icons/Memory';
import { useStyles } from '../styles';

const mainPages = [
  { displayName: 'Main State', name: 'main'},
  { displayName: 'Input History', name: 'input'},
  { displayName: 'Functions', name: 'functions'},
  { displayName: 'Resources', name: 'resources'},
  { displayName: 'Data Structures', name: 'data_structures'},
  { displayName: 'Game Information', name: 'game_info'},
];

export function RRDrawer( props ) {
  const memory_descriptors = props.memory_descriptors || [];
  const classes = useStyles();

  return <Drawer
  className={classes.drawer}
  variant="persistent"
  anchor="left"
  open={props.open}
  classes={{
    paper: classes.drawerPaper,
  }}
>
  <div className={classes.drawerHeader}>
    <IconButton onClick={props.handleDrawerClose}>
      {props.theme.direction === 'ltr' ? <ChevronLeftIcon /> : <ChevronRightIcon />}
    </IconButton>
  </div>
  <Divider />
  <List>
    {mainPages.map((page, index) => (
      <ListItem button key={page.name} onClick={()=>props.setCurrentTab(page.name)}>
        <ListItemIcon>{index % 2 === 0 ? <InboxIcon /> : <MailIcon />}</ListItemIcon>
        <ListItemText primary={page.displayName} />
      </ListItem>
    ))}
  </List>
  <Divider />
  <List>
    {memory_descriptors.map((memory_descriptor, index) => (
      <ListItem button key={memory_descriptor.name} onClick={()=>props.setCurrentTab("memory_"+memory_descriptor.name)}>
        <ListItemIcon><MemoryIcon /></ListItemIcon>
        <ListItemText primary={memory_descriptor.name} />
      </ListItem>
    ))}
  </List>
</Drawer>
}