// src/utils/localStorage.ts

import type { User, LocalStorageUser } from '../types';

// Local storage functions for user management

export const saveUser = (userData: LocalStorageUser): void => {
  const users = getUsers();
  users.push(userData);
  localStorage.setItem('labExpertUsers', JSON.stringify(users));
};

export const getUsers = (): LocalStorageUser[] => {
  const users = localStorage.getItem('labExpertUsers');
  return users ? JSON.parse(users) : [];
};

export const findUser = (email: string, password: string): LocalStorageUser | undefined => {
  const users = getUsers();
  return users.find(user => user.email === email && user.password === password);
};

export const checkEmailExists = (email: string): boolean => {
  const users = getUsers();
  return users.some(user => user.email === email);
};

export const updateUserPassword = (email: string, newPassword: string): void => {
  const users = getUsers();
  const updatedUsers = users.map(user => 
    user.email === email ? { ...user, password: newPassword } : user
  );
  localStorage.setItem('labExpertUsers', JSON.stringify(updatedUsers));
};

export const setCurrentUser = (user: LocalStorageUser): void => {
  localStorage.setItem('currentUser', JSON.stringify(user));
};

export const getCurrentUser = (): LocalStorageUser | null => {
  const user = localStorage.getItem('currentUser');
  return user ? JSON.parse(user) : null;
};

export const logout = (): void => {
  localStorage.removeItem('currentUser');
};